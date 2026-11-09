#include "WorkflowDocument.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLockFile>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QUuid>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>

namespace {
constexpr qint64 maxDocumentBytes = 2 * 1024 * 1024;
constexpr int maxRevision = std::numeric_limits<int>::max();
constexpr int maxDelayMs = 10000;
constexpr int maxTimeoutMs = 30000;
constexpr int maxTotalTimeoutMs = 1800000;

QByteArray hash(const QByteArray &bytes) {
    return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
}
bool fail(QString *error, const QString &message) {
    if (error) *error = message;
    return false;
}
QString uuid() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
QString pathKey(const QString &path) {
    if (path.isEmpty()) return {};
    const QFileInfo info(path);
    if (info.exists()) return info.canonicalFilePath();
    // Resolve the parent too: a not-yet-created file reached through a directory
    // symlink must use the same lock as its real path.
    const QString parent = info.dir().canonicalPath();
    return parent.isEmpty() ? QString{} : QDir(parent).absoluteFilePath(info.fileName());
}
std::filesystem::path nativePath(const QString &path) {
#ifdef Q_OS_WIN
    return std::filesystem::path(path.toStdWString());
#else
    return std::filesystem::path(QFile::encodeName(path).constData());
#endif
}
enum class DestinationIdentity { Different, Loaded, Unavailable };
DestinationIdentity loadedDestination(const QString &destination, const QString &loadedPath) {
    if (destination == loadedPath) return DestinationIdentity::Loaded;
    const QString currentLoadedPath = pathKey(loadedPath);
    if (!currentLoadedPath.isEmpty() && destination == currentLoadedPath) return DestinationIdentity::Loaded;
    // canonicalFilePath need not normalize casing on Windows. Do not simply
    // lowercase: case-sensitive volumes/directories may hold both spellings.
    const QString comparisonPath = currentLoadedPath.isEmpty() ? loadedPath : currentLoadedPath;
    if (destination.compare(comparisonPath, Qt::CaseInsensitive) != 0) return DestinationIdentity::Different;
    if (!QFileInfo::exists(destination) || !QFileInfo::exists(loadedPath)) {
        // Identity can no longer be verified after deletion. Fail closed for
        // a spelling-only Save As rather than recreating a deleted loaded file
        // or overwriting another file with a stale document.
        return DestinationIdentity::Loaded;
    }
    std::error_code error;
    const bool equivalent = std::filesystem::equivalent(nativePath(destination), nativePath(loadedPath), error);
    if (error) return DestinationIdentity::Unavailable;
    return equivalent ? DestinationIdentity::Loaded : DestinationIdentity::Different;
}
bool fileHash(const QString &path, QByteArray *result, QString *error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return fail(error, file.errorString());
    QCryptographicHash digest(QCryptographicHash::Sha256);
    if (!digest.addData(&file) || file.error() != QFileDevice::NoError)
        return fail(error, file.errorString());
    *result = digest.result();
    return true;
}
bool integer(const QJsonValue &value, double minimum, double maximum) {
    const double n = value.toDouble(-1);
    return value.isDouble() && std::isfinite(n) && n == std::floor(n) && n >= minimum && n <= maximum;
}
bool number(const QJsonValue &value, double minimum, double maximum) {
    const double n = value.toDouble(-1);
    return value.isDouble() && std::isfinite(n) && n >= minimum && n <= maximum;
}
bool identifier(const QJsonValue &value) {
    if (!value.isString()) return false;
    const QString text = value.toString();
    if (text.isEmpty() || text.size() > 128 || text.trimmed() != text) return false;
    for (QChar c : text) if (c.isSpace() || c.category() == QChar::Other_Control) return false;
    return true;
}
bool choice(const QJsonValue &value, std::initializer_list<const char *> choices) {
    if (!value.isString()) return false;
    for (const char *item : choices) if (value.toString() == QLatin1String(item)) return true;
    return false;
}
bool point(const QJsonValue &value) {
    const auto p = value.toObject();
    return value.isObject() && number(p.value(QStringLiteral("x")), 0, 1)
        && number(p.value(QStringLiteral("y")), 0, 1);
}
bool rect(const QJsonValue &value) {
    const auto r = value.toObject();
    const double w = r.value(QStringLiteral("width")).toDouble();
    const double h = r.value(QStringLiteral("height")).toDouble();
    return value.isObject() && point(value)
        && number(r.value(QStringLiteral("width")), 0, 1) && w > 0
        && number(r.value(QStringLiteral("height")), 0, 1) && h > 0
        && r.value(QStringLiteral("x")).toDouble() + w <= 1
        && r.value(QStringLiteral("y")).toDouble() + h <= 1;
}
bool referenceSize(const QJsonValue &value) {
    const auto size = value.toObject();
    return value.isObject() && integer(size.value(QStringLiteral("width")), 1, 65535)
        && integer(size.value(QStringLiteral("height")), 1, 65535);
}
QJsonObject positionObject(const QPointF &position, QJsonObject existing = {}) {
    existing.insert(QStringLiteral("x"), std::clamp(position.x(), 0.0, 1.0));
    existing.insert(QStringLiteral("y"), std::clamp(position.y(), 0.0, 1.0));
    return existing;
}
QJsonObject mergeObject(QJsonObject existing, const QJsonObject &patch) {
    for (auto it = patch.begin(); it != patch.end(); ++it) {
        existing.insert(it.key(), it.value().isObject() && existing.value(it.key()).isObject()
            ? QJsonValue(mergeObject(existing.value(it.key()).toObject(), it.value().toObject())) : it.value());
    }
    return existing;
}

// Extension objects can carry references too. Never allow deleting a point that
// is used in a nested condition, and do not silently ignore dangling references.
class Validator {
public:
    explicit Validator(bool running) : forRun(running) {}
    QStringList errors;
    QSet<QString> pointIds, layoutIds, checkIds, stepIds;
    bool forRun;

    void require(bool valid, const QString &path, const QString &message) {
        if (!valid) errors.append(path + QStringLiteral(": ") + message);
    }
    void text(const QJsonObject &object, const QString &key, const QString &path, bool nonempty = true) {
        const auto value = object.value(key);
        require(value.isString() && (!nonempty || !value.toString().trimmed().isEmpty()),
                path + QLatin1Char('.') + key, QStringLiteral("must be a string") + (nonempty ? QStringLiteral(" with a nonempty name") : QString{}));
    }
    void enumeration(const QJsonValue &value, std::initializer_list<const char *> allowed, const QString &path) {
        require(choice(value, allowed), path, QStringLiteral("unsupported or missing value"));
    }
    void whole(const QJsonValue &value, int low, int high, const QString &path) {
        require(integer(value, low, high), path, QStringLiteral("must be an integer in [%1, %2]").arg(low).arg(high));
    }
    QSet<QString> collect(const QJsonObject &data, const QString &key, int maximum) {
        const auto value = data.value(key);
        require(value.isArray(), key, QStringLiteral("must be an array"));
        const auto array = value.toArray();
        require(array.size() <= maximum, key, QStringLiteral("too many entries (maximum %1)").arg(maximum));
        QSet<QString> ids;
        for (qsizetype i = 0; i < array.size(); ++i) {
            const auto item = array.at(i).toObject();
            const QString path = key + QStringLiteral("[%1]").arg(i);
            require(array.at(i).isObject(), path, QStringLiteral("must be an object"));
            const auto id = item.value(QStringLiteral("id"));
            require(identifier(id), path + QStringLiteral(".id"), QStringLiteral("must be a nonempty identifier without whitespace"));
            require(!ids.contains(id.toString()), path + QStringLiteral(".id"), QStringLiteral("duplicate identifier"));
            if (identifier(id)) ids.insert(id.toString());
            text(item, QStringLiteral("name"), path);
        }
        return ids;
    }
    void reference(const QJsonValue &value, const QSet<QString> &ids, const QString &path, bool nullable) {
        if (nullable && value.isNull()) return;
        require(identifier(value) && ids.contains(value.toString()), path, QStringLiteral("must reference an existing ID") + (nullable ? QStringLiteral(" or be null") : QString{}));
    }
    void qualities(const QJsonObject &object, const QString &path) {
        if (!object.contains(QStringLiteral("originalQualities"))) return;
        const auto value = object.value(QStringLiteral("originalQualities"));
        const QString field = path + QStringLiteral(".originalQualities");
        require(value.isArray(), field, QStringLiteral("must be an array (empty means all qualities)"));
        QSet<QString> seen;
        for (const auto &quality : value.toArray()) {
            enumeration(quality, {"red", "yellow", "blue", "dark", "unknown"}, field);
            require(!seen.contains(quality.toString()), field, QStringLiteral("duplicate quality"));
            seen.insert(quality.toString());
        }
    }
    void references(const QJsonValue &value, const QString &path, int depth = 0) {
        if (depth > 64) { require(false, path, QStringLiteral("reference nesting exceeds 64 levels")); return; }
        if (value.isArray()) {
            const auto array = value.toArray();
            for (qsizetype i = 0; i < array.size(); ++i) references(array.at(i), path + QStringLiteral("[%1]").arg(i), depth + 1);
        } else if (value.isObject()) {
            const auto object = value.toObject();
            for (auto it = object.begin(); it != object.end(); ++it) {
                const QString childPath = path + QLatin1Char('.') + it.key();
                const QSet<QString> *ids = nullptr;
                if (it.key() == QStringLiteral("pointId") || it.key() == QStringLiteral("pointIds")) ids = &pointIds;
                if (it.key() == QStringLiteral("checkId") || it.key() == QStringLiteral("completionCheckId") || it.key() == QStringLiteral("entryCheckId") || it.key() == QStringLiteral("checkIds")) ids = &checkIds;
                if (it.key() == QStringLiteral("layoutId") || it.key() == QStringLiteral("layoutIds")) ids = &layoutIds;
                if (it.key() == QStringLiteral("stepId") || it.key() == QStringLiteral("stepIds")) ids = &stepIds;
                if (ids) {
                    if (it.key().endsWith(QStringLiteral("Ids"))) {
                        require(it.value().isArray(), childPath, QStringLiteral("must be an array of references"));
                        for (const auto &ref : it.value().toArray()) reference(ref, *ids, childPath, !forRun);
                    } else reference(it.value(), *ids, childPath, !forRun);
                }
                references(it.value(), childPath, depth + 1);
            }
        }
    }
};
bool referencesPoint(const QJsonValue &value, const QString &id, int depth = 0) {
    // Treat excessively deep extensions conservatively when removing geometry.
    if (depth > 64) return true;
    if (value.isArray()) {
        for (const auto &child : value.toArray()) if (referencesPoint(child, id, depth + 1)) return true;
    } else if (value.isObject()) {
        const auto object = value.toObject();
        if (object.value(QStringLiteral("pointId")) == id) return true;
        for (const auto &ref : object.value(QStringLiteral("pointIds")).toArray()) if (ref == id) return true;
        for (auto it = object.begin(); it != object.end(); ++it) if (referencesPoint(it.value(), id, depth + 1)) return true;
    }
    return false;
}
bool nameLess(const QString &a, const QString &b) {
    const QString an = QFileInfo(a).fileName(), bn = QFileInfo(b).fileName();
    const auto ak = an.normalized(QString::NormalizationForm_C).toCaseFolded().toUcs4();
    const auto bk = bn.normalized(QString::NormalizationForm_C).toCaseFolded().toUcs4();
    const auto at = ak == bk ? an.toUcs4() : ak, bt = ak == bk ? bn.toUcs4() : bk;
    return std::lexicographical_compare(at.begin(), at.end(), bt.begin(), bt.end());
}
}

QStringList WorkflowDocument::validate(const QJsonObject &data, bool forRun) {
    Validator v(forRun);
    v.require(data.value(QStringLiteral("kind")) == QStringLiteral("d4box.workflow"), QStringLiteral("kind"), QStringLiteral("must be d4box.workflow"));
    v.whole(data.value(QStringLiteral("schemaVersion")), 1, 1, QStringLiteral("schemaVersion"));
    static const QRegularExpression uuidPattern(QStringLiteral("^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$"));
    const auto id = data.value(QStringLiteral("id"));
    v.require(id.isString() && id.toString().size() == 36 && uuidPattern.match(id.toString()).hasMatch() && !QUuid(id.toString()).isNull(), QStringLiteral("id"), QStringLiteral("must be a non-null UUID without braces"));
    v.whole(data.value(QStringLiteral("revision")), 0, maxRevision, QStringLiteral("revision"));
    v.text(data, QStringLiteral("name"), QStringLiteral("document"));
    v.text(data, QStringLiteral("description"), QStringLiteral("document"), false);
    v.enumeration(data.value(QStringLiteral("status")), {"draft", "ready"}, QStringLiteral("status"));
    if (data.contains(QStringLiteral("provenance"))) v.require(data.value(QStringLiteral("provenance")).isObject(), QStringLiteral("provenance"), QStringLiteral("must be an object"));

    const QSet<QString> supported = {QStringLiteral("mouse.click.v1"), QStringLiteral("target.hover_item.v1"),
        QStringLiteral("target.inventory_once.v1"), QStringLiteral("target.preloaded.v1"),
        QStringLiteral("check.pixel.v1"), QStringLiteral("time.wait.v1"), QStringLiteral("condition.original_quality.v1")};
    const auto requirements = data.value(QStringLiteral("requires"));
    v.require(requirements.isArray(), QStringLiteral("requires"), QStringLiteral("must be an array"));
    QSet<QString> seenRequirements;
    for (const auto &requirement : requirements.toArray()) {
        v.require(identifier(requirement), QStringLiteral("requires"), QStringLiteral("capability names must be nonempty strings without whitespace"));
        v.require(!seenRequirements.contains(requirement.toString()), QStringLiteral("requires"), QStringLiteral("duplicate capability"));
        seenRequirements.insert(requirement.toString());
        if (forRun) v.require(supported.contains(requirement.toString()), QStringLiteral("requires"), QStringLiteral("unsupported capability: ") + requirement.toString());
    }
    if (forRun) for (const auto &required : derivedRequires(data))
        v.require(seenRequirements.contains(required), QStringLiteral("requires"), QStringLiteral("missing capability: ") + required);
    const auto sourceValue = data.value(QStringLiteral("source"));
    v.require(sourceValue.isObject(), QStringLiteral("source"), QStringLiteral("must be an object"));
    const auto source = sourceValue.toObject();
    const auto mode = source.value(QStringLiteral("mode"));
    v.enumeration(mode, {"hover_item", "inventory_once", "preloaded"}, QStringLiteral("source.mode"));
    v.whole(source.value(QStringLiteral("maxItems")), 1, mode == QStringLiteral("inventory_once") ? 33 : 1, QStringLiteral("source.maxItems"));
    v.qualities(source, QStringLiteral("source"));
    if (source.contains(QStringLiteral("excludedSlots"))) {
        const auto excluded = source.value(QStringLiteral("excludedSlots"));
        v.require(excluded.isArray(), QStringLiteral("source.excludedSlots"), QStringLiteral("must be an array of 0-based slots"));
        QSet<int> excludedIndices;
        for (const auto &slot : excluded.toArray()) {
            v.whole(slot, 0, 32, QStringLiteral("source.excludedSlots"));
            v.require(!excludedIndices.contains(slot.toInt(-1)), QStringLiteral("source.excludedSlots"), QStringLiteral("duplicate slot"));
            excludedIndices.insert(slot.toInt(-1));
        }
        if (forRun && mode == QStringLiteral("inventory_once")) v.require(excludedIndices.size() < 33, QStringLiteral("source.excludedSlots"), QStringLiteral("all inventory slots are excluded"));
    }
    const auto limitsValue = data.value(QStringLiteral("limits"));
    const auto limits = limitsValue.toObject();
    v.require(limitsValue.isObject(), QStringLiteral("limits"), QStringLiteral("must be an object"));
    v.whole(limits.value(QStringLiteral("maxItemsPerRun")), 1, 33, QStringLiteral("limits.maxItemsPerRun"));
    v.whole(limits.value(QStringLiteral("totalTimeoutMs")), 1000, maxTotalTimeoutMs, QStringLiteral("limits.totalTimeoutMs"));

    v.pointIds = v.collect(data, QStringLiteral("points"), 256);
    v.layoutIds = v.collect(data, QStringLiteral("layouts"), 32);
    v.checkIds = v.collect(data, QStringLiteral("checks"), 256);
    v.stepIds = v.collect(data, QStringLiteral("steps"), 100);
    v.require(!v.layoutIds.isEmpty(), QStringLiteral("layouts"), QStringLiteral("at least one layout is required"));
    v.reference(data.value(QStringLiteral("defaultLayoutId")), v.layoutIds, QStringLiteral("defaultLayoutId"), false);
    for (const auto &value : data.value(QStringLiteral("points")).toArray()) {
        const auto p = value.toObject();
        v.enumeration(p.value(QStringLiteral("role")), {"click", "check", "both"}, QStringLiteral("points.") + p.value(QStringLiteral("id")).toString() + QStringLiteral(".role"));
    }
    const auto layouts = data.value(QStringLiteral("layouts")).toArray();
    for (qsizetype i = 0; i < layouts.size(); ++i) {
        const auto layout = layouts.at(i).toObject();
        const QString path = QStringLiteral("layouts[%1]").arg(i);
        v.enumeration(layout.value(QStringLiteral("coordinateSpace")), {"client_normalized"}, path + QStringLiteral(".coordinateSpace"));
        const auto calibration = layout.value(QStringLiteral("calibration"));
        if (!calibration.isNull()) v.enumeration(calibration, {"unverified", "manual"}, path + QStringLiteral(".calibration"));
        const auto reference = layout.value(QStringLiteral("referenceClientPx"));
        v.require(reference.isNull() || referenceSize(reference), path + QStringLiteral(".referenceClientPx"), QStringLiteral("must be null or positive integer width/height up to 65535"));
        const auto inventoryValue = layout.value(QStringLiteral("inventory"));
        const auto inventory = inventoryValue.toObject();
        v.require(inventoryValue.isObject(), path + QStringLiteral(".inventory"), QStringLiteral("must be an object"));
        v.whole(inventory.value(QStringLiteral("rows")), 3, 3, path + QStringLiteral(".inventory.rows"));
        v.whole(inventory.value(QStringLiteral("columns")), 11, 11, path + QStringLiteral(".inventory.columns"));
        const auto rectangle = inventory.value(QStringLiteral("rect"));
        v.require(rectangle.isNull() || rect(rectangle), path + QStringLiteral(".inventory.rect"), QStringLiteral("must be null or a positive rectangle entirely within [0,1]"));
        const auto positionValue = layout.value(QStringLiteral("pointPositions"));
        v.require(positionValue.isObject(), path + QStringLiteral(".pointPositions"), QStringLiteral("must be an object"));
        const auto positions = positionValue.toObject();
        for (auto it = positions.begin(); it != positions.end(); ++it) {
            const QString p = path + QStringLiteral(".pointPositions.") + it.key();
            v.require(v.pointIds.contains(it.key()), p, QStringLiteral("unknown point ID"));
            v.require(it.value().isNull() || point(it.value()), p, QStringLiteral("must be null or numeric x/y within [0,1]"));
            if (forRun) v.require(point(it.value()), p, QStringLiteral("point position is not calibrated"));
        }
        for (const auto &pointId : v.pointIds) v.require(positions.contains(pointId), path + QStringLiteral(".pointPositions.") + pointId, QStringLiteral("missing position; use null while drafting"));
        if (forRun) {
            v.require(calibration == QStringLiteral("manual"), path + QStringLiteral(".calibration"), QStringLiteral("reference client requires manual confirmation (not game verification)"));
            v.require(referenceSize(reference), path + QStringLiteral(".referenceClientPx"), QStringLiteral("reference client size is not calibrated"));
            if (mode == QStringLiteral("inventory_once")) v.require(rect(rectangle), path + QStringLiteral(".inventory.rect"), QStringLiteral("inventory bounds are not calibrated"));
        }
    }
    static const QRegularExpression colorPattern(QStringLiteral("^#[0-9a-fA-F]{6}$"));
    QHash<QString, QJsonObject> checksById;
    const auto checks = data.value(QStringLiteral("checks")).toArray();
    for (qsizetype i = 0; i < checks.size(); ++i) {
        const auto check = checks.at(i).toObject();
        const QString path = QStringLiteral("checks[%1]").arg(i);
        checksById.insert(check.value(QStringLiteral("id")).toString(), check);
        v.enumeration(check.value(QStringLiteral("type")), {"pixel"}, path + QStringLiteral(".type"));
        v.reference(check.value(QStringLiteral("pointId")), v.pointIds, path + QStringLiteral(".pointId"), !forRun);
        const auto color = check.value(QStringLiteral("expectedColor"));
        v.require((!forRun && color.isNull()) || (color.isString() && color.toString().size() == 7 && colorPattern.match(color.toString()).hasMatch()), path + QStringLiteral(".expectedColor"), QStringLiteral("must be #RRGGBB (null allowed only in drafts)"));
        v.whole(check.value(QStringLiteral("tolerance")), 0, 255, path + QStringLiteral(".tolerance"));
        v.whole(check.value(QStringLiteral("stableSamples")), 1, 20, path + QStringLiteral(".stableSamples"));
        v.enumeration(check.value(QStringLiteral("mode")), {"present", "transition"}, path + QStringLiteral(".mode"));
    }
    if (data.contains(QStringLiteral("entryCheckId")) || forRun) {
        const auto entry = data.value(QStringLiteral("entryCheckId"));
        v.reference(entry, v.checkIds, QStringLiteral("entryCheckId"), !forRun);
        if (forRun && checksById.contains(entry.toString())) v.require(checksById.value(entry.toString()).value(QStringLiteral("mode")) == QStringLiteral("present"), QStringLiteral("entryCheckId"), QStringLiteral("entry check must use present mode"));
    }
    const auto steps = data.value(QStringLiteral("steps")).toArray();
    if (forRun) v.require(!steps.isEmpty(), QStringLiteral("steps"), QStringLiteral("at least one step is required"));
    for (qsizetype i = 0; i < steps.size(); ++i) {
        const auto step = steps.at(i).toObject();
        const QString path = QStringLiteral("steps[%1]").arg(i);
        const auto action = step.value(QStringLiteral("action"));
        v.enumeration(action, {"click", "wait", "check"}, path + QStringLiteral(".action"));
        v.qualities(step, path);
        if (action == QStringLiteral("wait") || action == QStringLiteral("click") || step.contains(QStringLiteral("minDelayMs")))
            v.whole(step.value(QStringLiteral("minDelayMs")), 0, maxDelayMs, path + QStringLiteral(".minDelayMs"));
        if (action == QStringLiteral("click") || action == QStringLiteral("check") || step.contains(QStringLiteral("timeoutMs")))
            v.whole(step.value(QStringLiteral("timeoutMs")), 100, maxTimeoutMs, path + QStringLiteral(".timeoutMs"));
        // Even fields left on a step after changing action must keep valid types.
        if (action == QStringLiteral("check")) v.reference(step.value(QStringLiteral("checkId")), v.checkIds, path + QStringLiteral(".checkId"), !forRun);
        if (action == QStringLiteral("click") || step.contains(QStringLiteral("button")))
            v.enumeration(step.value(QStringLiteral("button")), {"left", "right"}, path + QStringLiteral(".button"));
        if (action == QStringLiteral("click") || step.contains(QStringLiteral("risk")))
            v.enumeration(step.value(QStringLiteral("risk")), {"none", "mutating"}, path + QStringLiteral(".risk"));
        if (action == QStringLiteral("click") || step.contains(QStringLiteral("target"))) {
            const auto target = step.value(QStringLiteral("target"));
            if (!(target.isNull() && !forRun)) {
                v.require(target.isObject(), path + QStringLiteral(".target"), QStringLiteral("must be a target object (null allowed only in drafts)"));
                const auto object = target.toObject();
                const auto type = object.value(QStringLiteral("type"));
                v.enumeration(type, {"point", "current_item"}, path + QStringLiteral(".target.type"));
                if (type == QStringLiteral("point")) v.reference(object.value(QStringLiteral("pointId")), v.pointIds, path + QStringLiteral(".target.pointId"), !forRun);
                if (type == QStringLiteral("current_item")) {
                    v.require(!object.contains(QStringLiteral("pointId")), path + QStringLiteral(".target.pointId"), QStringLiteral("current_item must not carry a fixed point reference"));
                    if (forRun) v.require(mode != QStringLiteral("preloaded"), path + QStringLiteral(".target"), QStringLiteral("preloaded source has no current_item target"));
                }
            }
        }
        if (action == QStringLiteral("click") || step.contains(QStringLiteral("completionCheckId"))) {
            const auto completion = step.value(QStringLiteral("completionCheckId"));
            v.reference(completion, v.checkIds, path + QStringLiteral(".completionCheckId"), !forRun);
            if (forRun && action == QStringLiteral("click") && step.value(QStringLiteral("risk")) == QStringLiteral("mutating") && checksById.contains(completion.toString()))
                v.require(checksById.value(completion.toString()).value(QStringLiteral("mode")) == QStringLiteral("transition"), path + QStringLiteral(".completionCheckId"), QStringLiteral("mutating click requires a transition result check"));
        }
    }
    v.references(data.value(QStringLiteral("layouts")), QStringLiteral("layouts"));
    v.references(data.value(QStringLiteral("checks")), QStringLiteral("checks"));
    v.references(data.value(QStringLiteral("steps")), QStringLiteral("steps"));
    v.errors.removeDuplicates();
    return v.errors;
}

QStringList WorkflowDocument::derivedRequires(const QJsonObject &data) {
    QSet<QString> needed;
    const auto source = data.value(QStringLiteral("source")).toObject();
    const auto mode = source.value(QStringLiteral("mode"));
    if (choice(mode, {"hover_item", "inventory_once", "preloaded"}))
        needed.insert(QStringLiteral("target.") + mode.toString() + QStringLiteral(".v1"));
    if (!source.value(QStringLiteral("originalQualities")).toArray().isEmpty())
        needed.insert(QStringLiteral("condition.original_quality.v1"));
    for (const auto &value : data.value(QStringLiteral("checks")).toArray())
        if (value.toObject().value(QStringLiteral("type")) == QStringLiteral("pixel")) needed.insert(QStringLiteral("check.pixel.v1"));
    for (const auto &value : data.value(QStringLiteral("steps")).toArray()) {
        const auto step = value.toObject();
        const auto action = step.value(QStringLiteral("action"));
        if (action == QStringLiteral("click")) needed.insert(QStringLiteral("mouse.click.v1"));
        if (action == QStringLiteral("wait")) needed.insert(QStringLiteral("time.wait.v1"));
        if (action == QStringLiteral("check")) needed.insert(QStringLiteral("check.pixel.v1"));
        if (!step.value(QStringLiteral("originalQualities")).toArray().isEmpty()) needed.insert(QStringLiteral("condition.original_quality.v1"));
    }
    auto result = needed.values();
    result.sort();
    return result;
}

void WorkflowDocument::refreshRequirements() {
    // Invalid input stays visible for validation; never coerce a malformed
    // declaration or remove an extension just because this editor does not know it.
    if (!data.value(QStringLiteral("requires")).isArray()) return;
    auto requirements = data.value(QStringLiteral("requires")).toArray();
    bool changed = false;
    for (const auto &required : derivedRequires(data)) if (!requirements.contains(required)) {
        requirements.append(required); changed = true;
    }
    if (changed) { data.insert(QStringLiteral("requires"), requirements); dirty = true; }
}

WorkflowDocument WorkflowDocument::createDraft() {
    WorkflowDocument doc;
    doc.data = {
        {QStringLiteral("kind"), QStringLiteral("d4box.workflow")}, {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("id"), uuid()}, {QStringLiteral("revision"), 0},
        {QStringLiteral("name"), QStringLiteral("未命名流程")},
        {QStringLiteral("description"), QStringLiteral("示例布局，尚未校准；仅编辑草稿，不执行游戏操作。")},
        {QStringLiteral("status"), QStringLiteral("draft")}, {QStringLiteral("requires"), QJsonArray{}},
        {QStringLiteral("source"), QJsonObject{{QStringLiteral("mode"), QStringLiteral("hover_item")},
            {QStringLiteral("maxItems"), 1}, {QStringLiteral("originalQualities"), QJsonArray{}}, {QStringLiteral("excludedSlots"), QJsonArray{}}}},
        {QStringLiteral("points"), QJsonArray{
            QJsonObject{{QStringLiteral("id"), QStringLiteral("p1")}, {QStringLiteral("name"), QStringLiteral("示例点位 1")}, {QStringLiteral("role"), QStringLiteral("click")}},
            QJsonObject{{QStringLiteral("id"), QStringLiteral("p2")}, {QStringLiteral("name"), QStringLiteral("示例点位 2")}, {QStringLiteral("role"), QStringLiteral("click")}}}},
        {QStringLiteral("layouts"), QJsonArray{QJsonObject{
            {QStringLiteral("id"), QStringLiteral("default")}, {QStringLiteral("name"), QStringLiteral("示例布局（未校准）")},
            {QStringLiteral("coordinateSpace"), QStringLiteral("client_normalized")}, {QStringLiteral("calibration"), QStringLiteral("unverified")},
            {QStringLiteral("referenceClientPx"), QJsonValue::Null},
            {QStringLiteral("inventory"), QJsonObject{{QStringLiteral("rows"), 3}, {QStringLiteral("columns"), 11},
                {QStringLiteral("rect"), QJsonObject{{QStringLiteral("x"), 0.66}, {QStringLiteral("y"), 0.67}, {QStringLiteral("width"), 0.315}, {QStringLiteral("height"), 0.225}}}}},
            {QStringLiteral("pointPositions"), QJsonObject{{QStringLiteral("p1"), positionObject(QPointF(.30, .42))}, {QStringLiteral("p2"), positionObject(QPointF(.30, .78))}}}}}},
        {QStringLiteral("defaultLayoutId"), QStringLiteral("default")}, {QStringLiteral("checks"), QJsonArray{}},
        {QStringLiteral("entryCheckId"), QJsonValue::Null}, {QStringLiteral("steps"), QJsonArray{}},
        {QStringLiteral("limits"), QJsonObject{{QStringLiteral("maxItemsPerRun"), 1}, {QStringLiteral("totalTimeoutMs"), 60000}}}
    };
    doc.dirty = true;
    return doc;
}

QJsonObject WorkflowDocument::makeStep(const QString &action) {
    if (!choice(action, {"click", "wait", "check"})) return {};
    QJsonObject step{{QStringLiteral("id"), QStringLiteral("step_") + uuid()}, {QStringLiteral("action"), action},
        {QStringLiteral("originalQualities"), QJsonArray{}}};
    if (action == QStringLiteral("click")) {
        step.insert(QStringLiteral("name"), QStringLiteral("点击"));
        step.insert(QStringLiteral("button"), QStringLiteral("left"));
        step.insert(QStringLiteral("target"), QJsonValue::Null);
        step.insert(QStringLiteral("completionCheckId"), QJsonValue::Null);
        step.insert(QStringLiteral("risk"), QStringLiteral("mutating"));
    } else step.insert(QStringLiteral("name"), action == QStringLiteral("wait") ? QStringLiteral("等待") : QStringLiteral("检查"));
    if (action != QStringLiteral("check")) step.insert(QStringLiteral("minDelayMs"), 250);
    if (action != QStringLiteral("wait")) step.insert(QStringLiteral("timeoutMs"), 3000);
    if (action == QStringLiteral("check")) step.insert(QStringLiteral("checkId"), QJsonValue::Null);
    return step;
}

WorkflowDocument WorkflowDocument::createTransmuteDraft() {
    auto doc = createDraft();
    doc.data.insert(QStringLiteral("name"), QStringLiteral("自动嬗变 · 待校准草稿"));
    doc.data.insert(QStringLiteral("description"), QStringLiteral("迁入内置背包嬗变的红/黄品质分支与延时；默认仅红色，黄色分支可选。旧坐标为全屏参考，不是客户区校准。入口与结果检查均待填写；不执行游戏操作。"));
    doc.data.insert(QStringLiteral("requires"), QJsonArray{QStringLiteral("mouse.click.v1"), QStringLiteral("target.inventory_once.v1"), QStringLiteral("condition.original_quality.v1")});
    doc.data.insert(QStringLiteral("source"), QJsonObject{{QStringLiteral("mode"), QStringLiteral("inventory_once")},
        {QStringLiteral("maxItems"), 33}, {QStringLiteral("originalQualities"), QJsonArray{QStringLiteral("red")}}, {QStringLiteral("excludedSlots"), QJsonArray{}}});
    doc.data.insert(QStringLiteral("limits"), QJsonObject{{QStringLiteral("maxItemsPerRun"), 33}, {QStringLiteral("totalTimeoutMs"), 300000}});
    struct LegacyStep { const char *pointId; const char *name; const char *action; const char *quality; double x, y; int delay; };
    const LegacyStep legacy[] = {
        {"transmute_red", "transmute", "select", "red", .445, .380, 180},
        {"execute", "execute", "execute", "red", .168, .793, 450},
        {"confirm", "confirm", "confirm", "red", .432, .560, 1000},
        {"affix", "affix", "select", "yellow", .445, .233, 180},
        {"execute", "execute", "execute", "yellow", .168, .793, 450},
        {"confirm", "confirm", "confirm", "yellow", .432, .560, 1000},
        {"upgrade", "upgrade", "select", "yellow", .445, .720, 180},
        {"execute", "execute", "execute", "yellow", .168, .793, 450},
        {"confirm", "confirm", "confirm", "yellow", .432, .560, 1000},
        {"transmute_yellow", "transmute", "select", "yellow", .445, .350, 180},
        {"execute", "execute", "execute", "yellow", .168, .793, 450},
        {"confirm", "confirm", "confirm", "yellow", .432, .560, 1000},
        {"clear", "clear", "clear", "any", .168, .680, 350}
    };
    QJsonArray points, steps, legacySteps;
    QJsonObject positions;
    auto load = makeStep(QStringLiteral("click"));
    load.insert(QStringLiteral("id"), QStringLiteral("step_load"));
    load.insert(QStringLiteral("name"), QStringLiteral("装入当前物品"));
    load.insert(QStringLiteral("button"), QStringLiteral("right"));
    load.insert(QStringLiteral("target"), QJsonObject{{QStringLiteral("type"), QStringLiteral("current_item")}});
    steps.append(load);
    for (const auto &old : legacy) {
        const QString pointId = QString::fromLatin1(old.pointId), name = QString::fromLatin1(old.name);
        if (!positions.contains(pointId)) {
            points.append(QJsonObject{{QStringLiteral("id"), pointId}, {QStringLiteral("name"), name}, {QStringLiteral("role"), QStringLiteral("click")}});
            positions.insert(pointId, QJsonValue::Null);
        }
        auto step = makeStep(QStringLiteral("click"));
        step.insert(QStringLiteral("id"), QStringLiteral("step_%1").arg(steps.size()));
        step.insert(QStringLiteral("name"), name);
        step.insert(QStringLiteral("target"), QJsonObject{{QStringLiteral("type"), QStringLiteral("point")}, {QStringLiteral("pointId"), pointId}});
        step.insert(QStringLiteral("minDelayMs"), old.delay);
        if (QString::fromLatin1(old.quality) != QStringLiteral("any")) step.insert(QStringLiteral("originalQualities"), QJsonArray{QString::fromLatin1(old.quality)});
        steps.append(step);
        legacySteps.append(QJsonObject{{QStringLiteral("label"), name}, {QStringLiteral("action"), QString::fromLatin1(old.action)},
            {QStringLiteral("quality"), QString::fromLatin1(old.quality)}, {QStringLiteral("x"), old.x}, {QStringLiteral("y"), old.y},
            {QStringLiteral("delayMs"), old.delay}, {QStringLiteral("color"), QStringLiteral("-")},
            {QStringLiteral("tolerance"), 24}, {QStringLiteral("timeoutMs"), 3000}});
    }
    auto layout = doc.layout();
    layout.insert(QStringLiteral("name"), QStringLiteral("待校准布局"));
    layout.insert(QStringLiteral("inventory"), QJsonObject{{QStringLiteral("rows"), 3}, {QStringLiteral("columns"), 11}, {QStringLiteral("rect"), QJsonValue::Null}});
    layout.insert(QStringLiteral("pointPositions"), positions);
    doc.data.insert(QStringLiteral("points"), points);
    doc.data.insert(QStringLiteral("layouts"), QJsonArray{layout});
    doc.data.insert(QStringLiteral("steps"), steps);
    doc.data.insert(QStringLiteral("provenance"), QJsonObject{
        {QStringLiteral("source"), QStringLiteral("app/ahk/config/cube-workflows.ini")},
        {QStringLiteral("workflowId"), QStringLiteral("inventory_transmute")},
        {QStringLiteral("legacyCoordinateSpace"), QStringLiteral("screen")}, {QStringLiteral("legacyYellow"), true},
        {QStringLiteral("legacySteps"), legacySteps},
        {QStringLiteral("loadSource"), QStringLiteral("app/ahk/functions/CubeRunner.ahk: explicit right click + 250 ms")}});
    return doc;
}

bool WorkflowDocument::load(const QString &path, QString *error) {
    if (error) error->clear();
    const QString canonical = pathKey(path);
    if (canonical.isEmpty() || !QFileInfo(canonical).isFile()) return fail(error, QStringLiteral("配置文件不存在或不是普通文件。"));
    QFile file(canonical);
    if (!file.open(QIODevice::ReadOnly)) return fail(error, file.errorString());
    if (file.size() > maxDocumentBytes) return fail(error, QStringLiteral("配置超过 2 MiB 限制。"));
    const QByteArray bytes = file.read(maxDocumentBytes + 1);
    if (file.error() != QFileDevice::NoError) return fail(error, file.errorString());
    if (bytes.size() > maxDocumentBytes) return fail(error, QStringLiteral("配置超过 2 MiB 限制。"));
    QJsonParseError parseError;
    const QJsonDocument parsed = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !parsed.isObject()) return fail(error, QStringLiteral("不是有效的 JSON 对象：") + parseError.errorString());
    const auto errors = validate(parsed.object());
    if (!errors.isEmpty()) return fail(error, errors.join(QLatin1Char('\n')));
    data = parsed.object();
    filePath = canonical;
    loadedHash = hash(bytes);
    dirty = false;
    return true;
}

QStringList WorkflowDocument::catalog(const QString &directory, QStringList *invalid) {
    QStringList result;
    if (directory.isEmpty()) return result;
    const QDir dir(directory);
    QStringList paths;
    for (const QFileInfo &entry : dir.entryInfoList(QDir::Files, QDir::NoSort))
        if (!entry.isSymLink() && entry.fileName().endsWith(QStringLiteral(".json"), Qt::CaseInsensitive)) paths.append(entry.absoluteFilePath());
    std::sort(paths.begin(), paths.end(), nameLess);
    QHash<QString, QStringList> ids;
    for (const auto &path : paths) {
        WorkflowDocument candidate;
        QString error;
        if (candidate.load(path, &error)) {
            result.append(path);
            ids[candidate.data.value(QStringLiteral("id")).toString().toLower()].append(path);
        } else {
            QFile file(path);
            bool owned = path.endsWith(QStringLiteral(".d4workflow.json"), Qt::CaseInsensitive);
            if (file.open(QIODevice::ReadOnly) && file.size() <= maxDocumentBytes)
                owned |= QJsonDocument::fromJson(file.read(maxDocumentBytes + 1)).object().value(QStringLiteral("kind")) == QStringLiteral("d4box.workflow");
            if (owned && invalid) invalid->append(QFileInfo(path).fileName() + QStringLiteral(": ") + error);
        }
    }
    // A duplicated workflow UUID is not an arbitrary first-match selection.
    for (qsizetype i = result.size(); i-- > 0;) {
        bool duplicate = false;
        for (auto it = ids.begin(); it != ids.end(); ++it) if (it.value().size() > 1 && it.value().contains(result.at(i))) { duplicate = true; break; }
        if (duplicate) {
            if (invalid) invalid->append(QFileInfo(result.at(i)).fileName() + QStringLiteral(": duplicate workflow ID"));
            result.removeAt(i);
        }
    }
    return result;
}

bool WorkflowDocument::saveDraft(const QString &path, QString *error, bool overwrite) {
    if (error) error->clear();
    const auto errors = issues();
    if (!errors.isEmpty()) return fail(error, errors.join(QLatin1Char('\n')));
    if (data.value(QStringLiteral("revision")).toDouble() >= maxRevision) return fail(error, QStringLiteral("revision 已到上限，未写入。"));
    const QFileInfo requested(path);
    if (path.isEmpty() || (requested.isSymLink() && !requested.exists())) return fail(error, QStringLiteral("保存路径为空或符号链接目标不存在。"));
    const QString destination = pathKey(path);
    if (destination.isEmpty()) return fail(error, QStringLiteral("保存目录不存在。"));
    QLockFile lock(destination + QStringLiteral(".lock"));
    lock.setStaleLockTime(0);
    if (!lock.tryLock(0)) return fail(error, QStringLiteral("配置写锁不可用，文件可能正被其他编辑器保存：") + destination);
    if (pathKey(path) != destination || QFileInfo(destination).isSymLink()) return fail(error, QStringLiteral("保存路径在加锁时发生变化，未覆盖。"));
    const QFileInfo target(destination);
    const bool existed = target.exists();
    const auto identity = !loadedHash.isEmpty() && !filePath.isEmpty()
        ? loadedDestination(destination, filePath) : DestinationIdentity::Different;
    if (identity == DestinationIdentity::Unavailable) return fail(error, QStringLiteral("无法确认目标与已加载文件是否相同，未覆盖。"));
    const bool sameLoadedFile = identity == DestinationIdentity::Loaded;
    if (existed && !target.isFile()) return fail(error, QStringLiteral("目标不是普通文件。"));
    if (existed && !sameLoadedFile && !overwrite) return fail(error, QStringLiteral("目标文件已存在，请确认覆盖或选择新文件名。"));
    QByteArray observedHash;
    if (existed && !fileHash(destination, &observedHash, error)) return false;
    if (sameLoadedFile && (!existed || !QFileInfo::exists(filePath) || observedHash != loadedHash)) return fail(error, QStringLiteral("磁盘文件已被修改或删除，请重开或另存，未覆盖。"));
    if (existed && !target.isWritable()) return fail(error, QStringLiteral("目标文件只读，请另存为。"));
    QJsonObject next = data;
    next.insert(QStringLiteral("status"), QStringLiteral("draft"));
    next.insert(QStringLiteral("revision"), data.value(QStringLiteral("revision")).toInt() + 1);
    const QByteArray bytes = QJsonDocument(next).toJson(QJsonDocument::Indented);
    if (bytes.size() > maxDocumentBytes) return fail(error, QStringLiteral("配置超过 2 MiB 限制。"));
    QSaveFile file(destination);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) return fail(error, file.errorString());
    if (file.write(bytes) != bytes.size()) {
        const QString writeError = file.errorString();
        file.cancelWriting();
        return fail(error, writeError);
    }
    // The lock coordinates editors, while this second comparison also detects
    // noncooperating external edits made while the temporary file was written.
    QByteArray beforeCommit;
    const QFileInfo finalTarget(destination);
    const bool changed = pathKey(path) != destination || finalTarget.isSymLink()
        || finalTarget.exists() != existed
        || (existed && (!finalTarget.isFile() || !fileHash(destination, &beforeCommit, error) || beforeCommit != observedHash));
    if (changed) { file.cancelWriting(); return fail(error, QStringLiteral("写入期间目标已被修改或删除，未覆盖。")); }
    if (!file.commit()) return fail(error, file.errorString());
    data = next;
    filePath = destination;
    loadedHash = hash(bytes);
    dirty = false;
    return true;
}

QJsonObject WorkflowDocument::layout() const {
    for (const auto &value : data.value(QStringLiteral("layouts")).toArray())
        if (value.toObject().value(QStringLiteral("id")) == data.value(QStringLiteral("defaultLayoutId"))) return value.toObject();
    return {};
}

void WorkflowDocument::setLayout(const QJsonObject &patch) {
    const auto current = layout();
    if (current.isEmpty() || (patch.contains(QStringLiteral("id")) && patch.value(QStringLiteral("id")) != current.value(QStringLiteral("id")))) return;
    auto next = mergeObject(current, patch);
    if (next == current) return;
    const bool geometryChanged = next.value(QStringLiteral("pointPositions")) != current.value(QStringLiteral("pointPositions"))
        || next.value(QStringLiteral("inventory")) != current.value(QStringLiteral("inventory"))
        || next.value(QStringLiteral("referenceClientPx")) != current.value(QStringLiteral("referenceClientPx"));
    // Only an explicit manual confirmation in this patch may preserve the flag.
    if (geometryChanged && patch.value(QStringLiteral("calibration")) != QStringLiteral("manual")) next.insert(QStringLiteral("calibration"), QStringLiteral("unverified"));
    auto layouts = data.value(QStringLiteral("layouts")).toArray();
    for (qsizetype i = 0; i < layouts.size(); ++i) if (layouts.at(i).toObject().value(QStringLiteral("id")) == current.value(QStringLiteral("id"))) {
        layouts.replace(i, next); data.insert(QStringLiteral("layouts"), layouts); dirty = true; return;
    }
}

void WorkflowDocument::setPointPosition(const QString &id, const QPointF &position) {
    if (!std::isfinite(position.x()) || !std::isfinite(position.y())) return;
    bool exists = false;
    for (const auto &p : data.value(QStringLiteral("points")).toArray()) if (p.toObject().value(QStringLiteral("id")) == id) { exists = true; break; }
    if (!exists) return;
    auto current = layout();
    if (current.isEmpty()) return;
    auto positions = current.value(QStringLiteral("pointPositions")).toObject();
    const auto next = positionObject(position, positions.value(id).toObject());
    if (positions.value(id) == next) return;
    positions.insert(id, next);
    setLayout(QJsonObject{{QStringLiteral("pointPositions"), positions}, {QStringLiteral("calibration"), QStringLiteral("unverified")}});
}

void WorkflowDocument::setInventoryRect(const QRectF &rectangle) {
    if (!std::isfinite(rectangle.x()) || !std::isfinite(rectangle.y()) || !std::isfinite(rectangle.width()) || !std::isfinite(rectangle.height()) || rectangle.width() <= 0 || rectangle.height() <= 0) return;
    const auto current = layout();
    if (current.isEmpty()) return;
    auto inventory = current.value(QStringLiteral("inventory")).toObject();
    auto next = inventory.value(QStringLiteral("rect")).toObject();
    const double width = std::min(rectangle.width(), 1.0), height = std::min(rectangle.height(), 1.0);
    next.insert(QStringLiteral("x"), std::clamp(rectangle.x(), 0.0, 1.0 - width));
    next.insert(QStringLiteral("y"), std::clamp(rectangle.y(), 0.0, 1.0 - height));
    next.insert(QStringLiteral("width"), width); next.insert(QStringLiteral("height"), height);
    if (inventory.value(QStringLiteral("rect")) == next) return;
    inventory.insert(QStringLiteral("rect"), next);
    setLayout(QJsonObject{{QStringLiteral("inventory"), inventory}, {QStringLiteral("calibration"), QStringLiteral("unverified")}});
}

QString WorkflowDocument::addPoint(const QString &name, const QPointF &position) {
    if (name.trimmed().isEmpty() || !std::isfinite(position.x()) || !std::isfinite(position.y()) || layout().isEmpty()) return {};
    auto points = data.value(QStringLiteral("points")).toArray();
    if (points.size() >= 256) return {};
    const QString id = QStringLiteral("point_") + uuid();
    points.append(QJsonObject{{QStringLiteral("id"), id}, {QStringLiteral("name"), name}, {QStringLiteral("role"), QStringLiteral("click")}});
    auto layouts = data.value(QStringLiteral("layouts")).toArray();
    for (qsizetype i = 0; i < layouts.size(); ++i) {
        auto item = layouts.at(i).toObject();
        auto positions = item.value(QStringLiteral("pointPositions")).toObject();
        positions.insert(id, item.value(QStringLiteral("id")) == data.value(QStringLiteral("defaultLayoutId")) ? QJsonValue(positionObject(position)) : QJsonValue(QJsonValue::Null));
        item.insert(QStringLiteral("pointPositions"), positions);
        item.insert(QStringLiteral("calibration"), QStringLiteral("unverified"));
        layouts.replace(i, item);
    }
    data.insert(QStringLiteral("points"), points); data.insert(QStringLiteral("layouts"), layouts); dirty = true;
    return id;
}

bool WorkflowDocument::removePoint(const QString &id, QString *error) {
    if (error) error->clear();
    auto points = data.value(QStringLiteral("points")).toArray();
    qsizetype index = -1;
    for (qsizetype i = 0; i < points.size(); ++i) if (points.at(i).toObject().value(QStringLiteral("id")) == id) { index = i; break; }
    if (index < 0) return fail(error, QStringLiteral("点位不存在。"));
    if (referencesPoint(data.value(QStringLiteral("steps")), id) || referencesPoint(data.value(QStringLiteral("checks")), id) || referencesPoint(data.value(QStringLiteral("layouts")), id))
        return fail(error, QStringLiteral("步骤、检查或布局扩展仍引用此点位，请先移除引用。"));
    points.removeAt(index);
    auto layouts = data.value(QStringLiteral("layouts")).toArray();
    for (qsizetype i = 0; i < layouts.size(); ++i) {
        auto item = layouts.at(i).toObject();
        auto positions = item.value(QStringLiteral("pointPositions")).toObject();
        positions.remove(id); item.insert(QStringLiteral("pointPositions"), positions); layouts.replace(i, item);
    }
    data.insert(QStringLiteral("points"), points); data.insert(QStringLiteral("layouts"), layouts); dirty = true;
    return true;
}

void WorkflowDocument::renamePoint(const QString &id, const QString &name) {
    auto points = data.value(QStringLiteral("points")).toArray();
    for (qsizetype i = 0; i < points.size(); ++i) {
        auto p = points.at(i).toObject();
        if (p.value(QStringLiteral("id")) != id) continue;
        if (p.value(QStringLiteral("name")) == name) return;
        p.insert(QStringLiteral("name"), name); points.replace(i, p); data.insert(QStringLiteral("points"), points); dirty = true; return;
    }
}

void WorkflowDocument::setName(const QString &name) {
    if (data.value(QStringLiteral("name")) == name) return;
    data.insert(QStringLiteral("name"), name); dirty = true;
}
