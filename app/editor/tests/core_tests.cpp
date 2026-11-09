#include "WorkflowDocument.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLockFile>
#include <QProcess>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QTextStream>
#include <functional>
#include <limits>
#include <type_traits>

static_assert(std::is_copy_constructible<WorkflowDocument>::value, "snapshot documents must remain copyable");
static_assert(std::is_copy_assignable<WorkflowDocument>::value, "snapshot documents must remain assignable");

namespace {
struct Tests {
    int failures = 0, assertions = 0;
    void check(bool condition, const QString &message) {
        ++assertions;
        if (!condition) { ++failures; QTextStream(stderr) << "FAIL: " << message << '\n'; }
    }
    void write(const QString &path, const QByteArray &bytes) {
        QFile file(path);
        check(file.open(QIODevice::WriteOnly), "open fixture: " + path);
        check(file.write(bytes) == bytes.size(), "write fixture: " + path);
    }
    void write(const QString &path, const QJsonObject &object) { write(path, QJsonDocument(object).toJson()); }
    QByteArray read(const QString &path) {
        QFile file(path);
        check(file.open(QIODevice::ReadOnly), "read fixture: " + path);
        return file.readAll();
    }
    void invalid(const QJsonObject &object, const QString &field, bool forRun = false) {
        const auto errors = WorkflowDocument::validate(object, forRun);
        check(!errors.isEmpty() && errors.join('\n').contains(field), "invalid " + field + ": " + errors.join(';'));
    }
};
using Edit = std::function<void(QJsonObject &)>;
void editItem(QJsonObject &data, const QString &key, int index, const Edit &edit) {
    auto array = data.value(key).toArray(); auto item = array.at(index).toObject();
    edit(item); array.replace(index, item); data.insert(key, array);
}
void editObject(QJsonObject &data, const QString &key, const Edit &edit) {
    auto object = data.value(key).toObject(); edit(object); data.insert(key, object);
}
QJsonObject pixel(const QString &id, const QString &mode = "present") {
    return {{"id", id}, {"name", id}, {"type", "pixel"}, {"pointId", "p1"},
        {"expectedColor", "#9aC012"}, {"tolerance", 24}, {"stableSamples", 2}, {"mode", mode}};
}
QJsonObject runnable() {
    auto doc = WorkflowDocument::createDraft();
    doc.data.insert("checks", QJsonArray{pixel("entry"), pixel("result", "transition")});
    doc.data.insert("entryCheckId", "entry");
    auto step = WorkflowDocument::makeStep("click");
    step.insert("target", QJsonObject{{"type", "point"}, {"pointId", "p2"}});
    step.insert("completionCheckId", "result");
    doc.data.insert("steps", QJsonArray{step});
    doc.setLayout({{"calibration", "manual"}, {"referenceClientPx", QJsonObject{{"width", 1920}, {"height", 1080}}}});
    doc.refreshRequirements();
    return doc.data;
}

void validationTests(Tests &t) {
    const auto base = WorkflowDocument::createDraft().data;
    t.check(WorkflowDocument::validate(base).isEmpty(), "new draft has valid structure");
    const auto complete = runnable();
    t.check(WorkflowDocument::validate(complete, true).isEmpty(), "manual reference plus complete static checks passes forRun while still draft");
    t.check(complete.value("status") == "draft", "forRun does not require a ready claim");
    for (const QString field : {"kind", "schemaVersion", "id", "revision", "name", "description", "status", "requires", "source", "points", "layouts", "defaultLayoutId", "checks", "steps", "limits"}) {
        auto data = base; data.remove(field); t.invalid(data, field);
        data = base; data.insert(field, QJsonValue::Null); t.invalid(data, field);
        data = base; data.insert(field, true); t.invalid(data, field);
    }
    for (const auto &id : {"", "fixture", "00000000-0000-0000-0000-000000000000", "{5d605792-671a-4e53-890f-538c321b3908}"}) {
        auto data = base; data.insert("id", id); t.invalid(data, "id");
    }
    for (double revision : {-1.0, 0.5, double(std::numeric_limits<int>::max()) + 1}) {
        auto data = base; data.insert("revision", revision); t.invalid(data, "revision");
    }
    auto data = base; data.insert("revision", std::numeric_limits<int>::max());
    t.check(WorkflowDocument::validate(data).isEmpty(), "highest revision can be loaded even though it cannot be incremented");
    data = base; data.insert("schemaVersion", 1.5); t.invalid(data, "schemaVersion");
    data = base; data.insert("status", "published"); t.invalid(data, "status");
    data = base; data.insert("name", " \t "); t.invalid(data, "name");
    data = base; data.insert("provenance", "not an object"); t.invalid(data, "provenance");
    data = complete; data.insert("requires", QJsonArray{"future.extension.v99"});
    t.check(WorkflowDocument::validate(data).isEmpty(), "unknown requirements preserved in drafts");
    t.invalid(data, "requires", true);
    data = base; data.insert("requires", QJsonArray{"mouse.click.v1", "mouse.click.v1"}); t.invalid(data, "requires");
    data = base; data.insert("requires", QJsonArray{42}); t.invalid(data, "requires");
    data = complete; data.insert("requires", QJsonArray{"mouse.click.v1", "target.hover_item.v1", "target.inventory_once.v1", "target.preloaded.v1", "check.pixel.v1", "time.wait.v1", "condition.original_quality.v1"});
    t.check(WorkflowDocument::validate(data, true).isEmpty(), "all supported capabilities accepted");

    for (const auto &mode : {"hover_item", "inventory_once", "preloaded"}) {
        data = base; editObject(data, "source", [&](auto &source) { source.insert("mode", mode); source.insert("maxItems", QString(mode) == "inventory_once" ? 33 : 1); });
        t.check(WorkflowDocument::validate(data).isEmpty(), "supported source: " + QString(mode));
    }
    for (const QJsonValue value : {QJsonValue(0), QJsonValue(-1), QJsonValue(1.5), QJsonValue(34), QJsonValue("1"), QJsonValue(true)}) {
        data = base; editObject(data, "source", [&](auto &s) { s.insert("mode", "inventory_once"); s.insert("maxItems", value); }); t.invalid(data, "source.maxItems");
    }
    data = base; editObject(data, "source", [](auto &s) { s.insert("mode", "forever"); }); t.invalid(data, "source.mode");
    data = base; editObject(data, "source", [](auto &s) { s.insert("maxItems", 2); }); t.invalid(data, "source.maxItems");
    for (const auto &qualities : {QJsonArray{}, QJsonArray{"red", "yellow", "blue", "dark", "unknown"}}) {
        data = base; editObject(data, "source", [&](auto &s) { s.insert("originalQualities", qualities); });
        auto step = WorkflowDocument::makeStep("wait"); step.insert("originalQualities", qualities); data.insert("steps", QJsonArray{step});
        t.check(WorkflowDocument::validate(data).isEmpty(), "empty/all supported original quality labels are valid");
    }
    for (const QJsonValue qualities : {QJsonValue("red"), QJsonValue(QJsonArray{"red", "red"}), QJsonValue(QJsonArray{"purple"}), QJsonValue(QJsonArray{1}), QJsonValue(QJsonValue::Null)}) {
        data = base; editObject(data, "source", [&](auto &s) { s.insert("originalQualities", qualities); }); t.invalid(data, "originalQualities");
    }
    data = base; editObject(data, "source", [](auto &s) { s.insert("excludedSlots", QJsonArray{0, 32}); });
    t.check(WorkflowDocument::validate(data).isEmpty(), "excluded slots include 0 and 32");
    for (const QJsonValue excluded : {QJsonValue(QJsonArray{-1}), QJsonValue(QJsonArray{33}), QJsonValue(QJsonArray{0.5}), QJsonValue(QJsonArray{0, 0}), QJsonValue(QJsonArray{"0"}), QJsonValue(true)}) {
        data = base; editObject(data, "source", [&](auto &s) { s.insert("excludedSlots", excluded); }); t.invalid(data, "excludedSlots");
    }
    data = complete; editObject(data, "source", [](auto &s) { QJsonArray excluded; for (int i = 0; i < 33; ++i) excluded.append(i); s.insert("mode", "inventory_once"); s.insert("excludedSlots", excluded); });
    t.check(WorkflowDocument::validate(data).isEmpty(), "all excluded slots are saveable as a draft"); t.invalid(data, "excludedSlots", true);
    for (const QString field : {"maxItemsPerRun", "totalTimeoutMs"}) {
        for (const QJsonValue value : {QJsonValue(0), QJsonValue(1.1), QJsonValue("1000"), QJsonValue(1800001)}) {
            data = base; editObject(data, "limits", [&](auto &s) { s.insert(field, value); }); t.invalid(data, "limits." + field);
        }
    }
    data = base; editObject(data, "limits", [](auto &s) { s.insert("maxItemsPerRun", 33); s.insert("totalTimeoutMs", 1800000); });
    t.check(WorkflowDocument::validate(data).isEmpty(), "finite limit upper boundaries accepted");

    for (const QString array : {"points", "layouts", "checks", "steps"}) {
        data = complete; auto entries = data.value(array).toArray(); entries.append(entries.first()); data.insert(array, entries); t.invalid(data, "duplicate identifier");
        data = complete; editItem(data, array, 0, [](auto &o) { o.insert("id", " "); }); t.invalid(data, array);
        data = complete; editItem(data, array, 0, [](auto &o) { o.insert("id", "two words"); }); t.invalid(data, array);
        data = complete; editItem(data, array, 0, [](auto &o) { o.remove("name"); }); t.invalid(data, array);
        data = complete; data.insert(array, QJsonArray{true}); t.invalid(data, array);
    }
    data = base; data.insert("defaultLayoutId", "missing"); t.invalid(data, "defaultLayoutId");
    data = base; editItem(data, "points", 0, [](auto &p) { p.insert("role", "code"); }); t.invalid(data, "role");
    data = base; editItem(data, "layouts", 0, [](auto &l) { l.insert("coordinateSpace", "screen"); }); t.invalid(data, "coordinateSpace");
    data = base; editItem(data, "layouts", 0, [](auto &l) { l.insert("calibration", "verified"); }); t.invalid(data, "calibration");
    data = base; editItem(data, "layouts", 0, [](auto &l) { l.insert("calibration", QJsonValue::Null); l.insert("referenceClientPx", QJsonValue::Null); editObject(l, "inventory", [](auto &i) { i.insert("rect", QJsonValue::Null); }); editObject(l, "pointPositions", [](auto &p) { p.insert("p1", QJsonValue::Null); }); });
    t.check(WorkflowDocument::validate(data).isEmpty(), "uncalibrated null geometry remains a structural draft");
    t.invalid(data, "calibration", true);
    for (const QJsonValue size : {QJsonValue(QJsonArray{1920, 1080}), QJsonValue(QJsonObject{{"width", 0}, {"height", 1080}}), QJsonValue(QJsonObject{{"width", 1920.5}, {"height", 1080}}), QJsonValue(QJsonObject{{"width", 1920}, {"height", "1080"}}), QJsonValue(QJsonObject{{"width", 65536}, {"height", 1080}})}) {
        data = base; editItem(data, "layouts", 0, [&](auto &l) { l.insert("referenceClientPx", size); }); t.invalid(data, "referenceClientPx");
    }
    for (const auto &field : {"x", "y"}) {
        for (const QJsonValue value : {QJsonValue(-.01), QJsonValue(1.01), QJsonValue("0.5"), QJsonValue(true), QJsonValue(QJsonValue::Null), QJsonValue(std::numeric_limits<double>::infinity()), QJsonValue(std::numeric_limits<double>::quiet_NaN())}) {
            data = base; editItem(data, "layouts", 0, [&](auto &l) { editObject(l, "pointPositions", [&](auto &p) { editObject(p, "p1", [&](auto &pos) { pos.insert(field, value); }); }); }); t.invalid(data, "pointPositions.p1");
        }
    }
    data = base; editItem(data, "layouts", 0, [](auto &l) { editObject(l, "pointPositions", [](auto &p) { p.insert("p1", QJsonObject{{"x", 1}, {"y", 0}}); }); });
    t.check(WorkflowDocument::validate(data).isEmpty(), "point boundary values 0 and 1 allowed");
    data = base; editItem(data, "layouts", 0, [](auto &l) { editObject(l, "pointPositions", [](auto &p) { p.remove("p1"); }); }); t.invalid(data, "missing position");
    data = base; editItem(data, "layouts", 0, [](auto &l) { editObject(l, "pointPositions", [](auto &p) { p.insert("nonexistent", QJsonValue::Null); }); }); t.invalid(data, "unknown point ID");
    for (const QJsonObject invalidRect : {QJsonObject{{"x", 0}, {"y", 0}, {"width", 0}, {"height", .1}}, QJsonObject{{"x", .9}, {"y", .5}, {"width", .2}, {"height", .1}}, QJsonObject{{"x", 0}, {"y", .9}, {"width", .1}, {"height", .2}}, QJsonObject{{"x", 0}, {"y", 0}, {"width", .1}}, QJsonObject{{"x", -.1}, {"y", 0}, {"width", .1}, {"height", .1}}}) {
        data = base; editItem(data, "layouts", 0, [&](auto &l) { editObject(l, "inventory", [&](auto &i) { i.insert("rect", invalidRect); }); }); t.invalid(data, "inventory.rect");
    }
    for (const auto &field : {"rows", "columns"}) {
        data = base; editItem(data, "layouts", 0, [&](auto &l) { editObject(l, "inventory", [&](auto &i) { i.insert(field, 4); }); }); t.invalid(data, "inventory." + QString(field));
    }
    auto blank = base; blank.insert("points", QJsonArray{}); editItem(blank, "layouts", 0, [](auto &l) { l.insert("pointPositions", QJsonObject{}); });
    t.check(WorkflowDocument::validate(blank).isEmpty(), "empty point list supports an incomplete new draft");

    for (const auto &action : {"click", "wait", "check"}) {
        data = base; auto step = WorkflowDocument::makeStep(action); data.insert("steps", QJsonArray{step});
        t.check(WorkflowDocument::validate(data).isEmpty(), "makeStep creates a saveable draft: " + QString(action));
        t.check(step.value("id") != WorkflowDocument::makeStep(action).value("id"), "step IDs unique");
        t.check(step.value("originalQualities").toArray().isEmpty(), "step defaults to all original qualities");
        if (QString(action) != "check") {
            for (const QJsonValue ms : {QJsonValue(-1), QJsonValue(10001), QJsonValue(.5), QJsonValue("250")}) {
                auto bad = data; editItem(bad, "steps", 0, [&](auto &s) { s.insert("minDelayMs", ms); }); t.invalid(bad, "minDelayMs");
            }
        }
        if (QString(action) != "wait") {
            for (const QJsonValue ms : {QJsonValue(0), QJsonValue(99), QJsonValue(30001), QJsonValue(.5), QJsonValue("3000")}) {
                auto bad = data; editItem(bad, "steps", 0, [&](auto &s) { s.insert("timeoutMs", ms); }); t.invalid(bad, "timeoutMs");
            }
        }
    }
    t.check(WorkflowDocument::makeStep("shell").isEmpty(), "unknown actions cannot be generated");
    for (const auto &action : {"shell", "execute", "loop", "", "CLICK"}) {
        data = complete; editItem(data, "steps", 0, [&](auto &s) { s.insert("action", action); }); t.invalid(data, "action");
    }
    for (const auto &button : {"middle", "", "LEFT"}) {
        data = complete; editItem(data, "steps", 0, [&](auto &s) { s.insert("button", button); }); t.invalid(data, "button");
    }
    data = complete; editItem(data, "steps", 0, [](auto &s) { s.insert("risk", "unchecked"); }); t.invalid(data, "risk");
    data = complete; editItem(data, "steps", 0, [](auto &s) { s.insert("target", QJsonObject{{"type", "point"}, {"pointId", "missing"}}); }); t.invalid(data, "pointId");
    data = complete; editItem(data, "steps", 0, [](auto &s) { s.insert("target", QJsonObject{{"type", "screen"}, {"x", 1}, {"y", 1}}); }); t.invalid(data, "target.type");
    data = complete; editItem(data, "steps", 0, [](auto &s) { s.insert("target", QJsonArray{}); }); t.invalid(data, "target");
    data = complete; editItem(data, "steps", 0, [](auto &s) { s.insert("target", QJsonValue::Null); s.insert("completionCheckId", QJsonValue::Null); });
    t.check(WorkflowDocument::validate(data).isEmpty(), "null click target and completion check allowed in drafts"); t.invalid(data, "target", true); t.invalid(data, "completionCheckId", true);
    data = complete; editItem(data, "steps", 0, [](auto &s) { s.insert("button", "right"); s.insert("target", QJsonObject{{"type", "current_item"}}); });
    t.check(WorkflowDocument::validate(data, true).isEmpty(), "right-click current_item is supported");
    editObject(data, "source", [](auto &s) { s.insert("mode", "preloaded"); }); t.invalid(data, "current_item", true);
    data = complete; editItem(data, "steps", 0, [](auto &s) { s.insert("completionCheckId", "missing"); }); t.invalid(data, "completionCheckId");
    data = complete; data.insert("entryCheckId", "result"); t.invalid(data, "entry check", true);
    data = complete; data.remove("entryCheckId"); t.check(WorkflowDocument::validate(data).isEmpty(), "entryCheckId optional in drafts"); t.invalid(data, "entryCheckId", true);
    data = complete; editItem(data, "steps", 0, [](auto &s) { s.insert("completionCheckId", "entry"); }); t.invalid(data, "transition", true);
    data = complete; data.insert("steps", QJsonArray{}); t.invalid(data, "steps", true);
    auto checkStep = WorkflowDocument::makeStep("check"); checkStep.insert("checkId", "entry"); data = complete; data.insert("steps", QJsonArray{checkStep, WorkflowDocument::makeStep("wait")});
    auto withWaitCapability = data.value("requires").toArray(); withWaitCapability.append("time.wait.v1"); data.insert("requires", withWaitCapability);
    t.check(WorkflowDocument::validate(data, true).isEmpty(), "check and wait validate independently of clicks");
    editItem(data, "steps", 0, [](auto &s) { s.insert("checkId", "missing"); }); t.invalid(data, "checkId");

    for (const auto &color : {"ABCDEF", "#123", "#12GGff", "#12345678", "red", "#123456\n"}) {
        data = complete; editItem(data, "checks", 0, [&](auto &c) { c.insert("expectedColor", color); }); t.invalid(data, "expectedColor");
    }
    data = complete; editItem(data, "checks", 0, [](auto &c) { c.insert("pointId", QJsonValue::Null); c.insert("expectedColor", QJsonValue::Null); });
    t.check(WorkflowDocument::validate(data).isEmpty(), "pending pixel point and color accepted"); t.invalid(data, "expectedColor", true);
    data = complete; editItem(data, "checks", 0, [](auto &c) { c.insert("type", "script"); }); t.invalid(data, "type");
    data = complete; editItem(data, "checks", 0, [](auto &c) { c.insert("mode", "sometimes"); }); t.invalid(data, "mode");
    for (const QJsonValue tolerance : {QJsonValue(-1), QJsonValue(256), QJsonValue(.5), QJsonValue("24")}) {
        data = complete; editItem(data, "checks", 0, [&](auto &c) { c.insert("tolerance", tolerance); }); t.invalid(data, "tolerance");
    }
    for (const QJsonValue samples : {QJsonValue(0), QJsonValue(21), QJsonValue(1.5), QJsonValue(true)}) {
        data = complete; editItem(data, "checks", 0, [&](auto &c) { c.insert("stableSamples", samples); }); t.invalid(data, "stableSamples");
    }
    for (const auto &key : {"pointId", "checkId", "layoutId", "stepId"}) {
        data = complete; editItem(data, "steps", 0, [&](auto &s) { s.insert("extension", QJsonArray{QJsonObject{{"condition", QJsonArray{QJsonObject{{key, "missing"}}}}}}); }); t.invalid(data, key);
    }
    data = complete; editItem(data, "checks", 0, [](auto &s) { s.insert("extension", QJsonObject{{"pointIds", QJsonArray{"p1", "missing"}}}); }); t.invalid(data, "pointIds");
    data = complete; editItem(data, "layouts", 0, [](auto &l) { l.insert("extension", QJsonObject{{"checkId", "missing"}}); }); t.invalid(data, "checkId");
    data = complete; editItem(data, "steps", 0, [](auto &s) { QJsonValue nested = QJsonObject{{"pointId", "p1"}}; for (int i = 0; i < 66; ++i) nested = QJsonObject{{"nested", nested}}; s.insert("extension", nested); }); t.invalid(data, "nesting");
    data = complete; data.insert("futureMetadata", QJsonObject{{"preserve", QJsonArray{1, "value", true, QJsonValue::Null}}});
    t.check(WorkflowDocument::validate(data, true).isEmpty(), "opaque unknown metadata is not discarded or mistaken for an action");
}

void mutationTests(Tests &t) {
    auto doc = WorkflowDocument::createDraft(); doc.dirty = false;
    const auto initial = doc.data;
    doc.setName(doc.data.value("name").toString());
    doc.setPointPosition("p1", QPointF(.30, .42));
    doc.setPointPosition("missing", QPointF(.5, .5));
    doc.setPointPosition("p1", QPointF(std::numeric_limits<double>::infinity(), .5));
    doc.setInventoryRect(QRectF(.66, .67, .315, .225));
    doc.setInventoryRect(QRectF(0, 0, -1, .2));
    doc.setInventoryRect(QRectF(0, 0, .2, 0));
    doc.setInventoryRect(QRectF(0, 0, .2, std::numeric_limits<double>::quiet_NaN()));
    doc.setLayout(doc.layout()); doc.setLayout({}); doc.setLayout({{"id", "other"}});
    doc.renamePoint("p1", "示例点位 1"); doc.renamePoint("missing", "ignored");
    t.check(doc.data == initial && !doc.dirty, "no-op/invalid geometry mutations do not dirty or change JSON");
    QString error;
    t.check(!doc.removePoint("missing", &error) && !error.isEmpty() && !doc.dirty, "missing point removal is an explained no-op");
    t.check(doc.addPoint("bad", QPointF(std::numeric_limits<double>::quiet_NaN(), 0)).isEmpty() && !doc.dirty, "nonfinite new point rejected");
    t.check(doc.addPoint("  ", QPointF(.5, .5)).isEmpty() && !doc.dirty, "blank new point name rejected");
    doc.setPointPosition("p1", QPointF(2, -1));
    auto p = doc.layout().value("pointPositions").toObject().value("p1").toObject();
    t.check(p.value("x") == 1 && p.value("y") == 0 && doc.dirty, "point drag clamps to bounds");
    doc.setInventoryRect(QRectF(-.2, .95, .4, .3));
    auto r = doc.layout().value("inventory").toObject().value("rect").toObject();
    t.check(r.value("x") == 0 && r.value("y") == .7 && r.value("width") == .4 && r.value("height") == .3, "rect translates within bounds without shrinking unnecessarily");
    doc.setInventoryRect(QRectF(.2, -.5, 2, 2));
    r = doc.layout().value("inventory").toObject().value("rect").toObject();
    t.check(r.value("x") == 0 && r.value("y") == 0 && r.value("width") == 1 && r.value("height") == 1, "oversize rect clamps to client");
    t.check(doc.issues().isEmpty(), "clamped geometry satisfies validator");
    doc.setLayout({{"calibration", "manual"}, {"referenceClientPx", QJsonObject{{"width", 1920}, {"height", 1080}}}, {"futureLayout", true}});
    t.check(doc.layout().value("calibration") == "manual" && doc.layout().value("futureLayout").toBool(), "layout patch records manual confirmation and preserves extensions");
    doc.dirty = false; doc.setPointPosition("p1", QPointF(1, 0));
    t.check(!doc.dirty && doc.layout().value("calibration") == "manual", "unchanged coordinates do not invalidate calibration");
    doc.setPointPosition("p1", QPointF(.5, .5));
    t.check(doc.layout().value("calibration") == "unverified", "point change invalidates manual calibration");
    doc.setLayout({{"calibration", "manual"}}); doc.setInventoryRect(QRectF(.1, .2, .3, .4));
    t.check(doc.layout().value("calibration") == "unverified", "rect change invalidates manual calibration");
    doc.setLayout({{"calibration", "manual"}}); doc.setLayout({{"referenceClientPx", QJsonObject{{"width", 1280}}}});
    t.check(doc.layout().value("calibration") == "unverified" && doc.layout().value("referenceClientPx").toObject().value("height") == 1080, "reference change invalidates calibration and layout patch preserves unmentioned fields");
    editItem(doc.data, "layouts", 0, [](auto &l) { editObject(l, "pointPositions", [](auto &positions) { editObject(positions, "p1", [](auto &point) { point.insert("futurePoint", 1); }); }); editObject(l, "inventory", [](auto &i) { i.insert("futureGrid", true); editObject(i, "rect", [](auto &r) { r.insert("futureRect", true); }); }); });
    doc.setPointPosition("p1", QPointF(.6, .4)); doc.setInventoryRect(QRectF(.2, .2, .3, .4));
    t.check(doc.layout().value("pointPositions").toObject().value("p1").toObject().value("futurePoint") == 1, "coordinate setter preserves unknown point geometry fields");
    t.check(doc.layout().value("inventory").toObject().value("futureGrid").toBool() && doc.layout().value("inventory").toObject().value("rect").toObject().value("futureRect").toBool(), "rect setter preserves nested extensions");
    auto alternate = doc.layout(); alternate.insert("id", "other"); auto layouts = doc.data.value("layouts").toArray(); layouts.append(alternate); doc.data.insert("layouts", layouts);
    const auto newId = doc.addPoint("新点位", QPointF(.4, .2));
    t.check(!newId.isEmpty() && doc.issues().isEmpty(), "add point assigns ID and all layouts remain valid");
    t.check(doc.data.value("layouts").toArray().last().toObject().value("pointPositions").toObject().value(newId).isNull(), "new point is uncalibrated in alternate layouts");
    const auto positions = doc.layout().value("pointPositions");
    doc.renamePoint(newId, "重命名");
    t.check(doc.data.value("points").toArray().last().toObject().value("id") == newId && doc.layout().value("pointPositions") == positions, "rename keeps stable point ID and geometry");
    auto step = WorkflowDocument::makeStep("click"); step.insert("target", QJsonObject{{"type", "point"}, {"pointId", newId}}); doc.data.insert("steps", QJsonArray{step}); doc.dirty = false;
    auto before = doc.data;
    t.check(!doc.removePoint(newId, &error) && doc.data == before && !doc.dirty, "direct step reference blocks removal transactionally");
    step = WorkflowDocument::makeStep("wait"); step.insert("futureCondition", QJsonArray{QJsonObject{{"all", QJsonArray{QJsonObject{{"pointId", newId}}}}}}); doc.data.insert("steps", QJsonArray{step}); before = doc.data;
    t.check(!doc.removePoint(newId, &error) && doc.data == before && !doc.dirty, "nested step reference blocks removal");
    doc.data.insert("steps", QJsonArray{}); auto check = pixel("guard"); check.insert("futureCondition", QJsonArray{QJsonObject{{"pointIds", QJsonArray{newId}}}}); doc.data.insert("checks", QJsonArray{check});
    t.check(!doc.removePoint(newId, &error), "nested check reference array blocks removal");
    check.remove("futureCondition"); check.insert("pointId", newId); doc.data.insert("checks", QJsonArray{check});
    t.check(!doc.removePoint(newId, &error), "pixel check reference blocks removal");
    doc.data.insert("checks", QJsonArray{}); step = WorkflowDocument::makeStep("wait"); step.insert("name", newId); doc.data.insert("steps", QJsonArray{step});
    t.check(doc.removePoint(newId, &error) && error.isEmpty() && doc.dirty, "ordinary text matching ID is not a reference");
    for (const auto &value : doc.data.value("layouts").toArray()) t.check(!value.toObject().value("pointPositions").toObject().contains(newId), "point deletion removes all layout positions");
    auto snapshot = doc; snapshot.setName("snapshot rename");
    t.check(snapshot.data != doc.data && snapshot.filePath == doc.filePath && doc.data.value("name") != "snapshot rename", "copying document isolates JSON mutations");
    snapshot = doc; t.check(snapshot.data == doc.data && snapshot.dirty == doc.dirty, "copy assignment supports snapshot undo");
}

void requirementTests(Tests &t) {
    auto doc = WorkflowDocument::createDraft(); doc.dirty = false;
    const auto originalId = doc.data.value("id"), originalRevision = doc.data.value("revision");
    t.check(WorkflowDocument::derivedRequires(doc.data) == QStringList{"target.hover_item.v1"}, "empty draft derives only its selected source capability");
    doc.refreshRequirements();
    t.check(doc.dirty && doc.data.value("requires") == QJsonArray{"target.hover_item.v1"}, "explicit refresh appends missing requirements");
    const auto baseline = doc.data; doc.dirty = false; doc.refreshRequirements();
    t.check(!doc.dirty && doc.data == baseline, "repeated capability refresh is a no-op");
    doc.data.insert("requires", QJsonArray{"future.extension.v2", "mouse.click.v1"});
    auto click = WorkflowDocument::makeStep("click"); click.insert("originalQualities", QJsonArray{"red", "dark", "unknown"});
    doc.data.insert("steps", QJsonArray{click, WorkflowDocument::makeStep("wait"), WorkflowDocument::makeStep("check")});
    doc.refreshRequirements();
    t.check(doc.data.value("requires") == QJsonArray{"future.extension.v2", "mouse.click.v1", "check.pixel.v1", "condition.original_quality.v1", "target.hover_item.v1", "time.wait.v1"}, "capability union preserves unknown declarations, existing order and mixed quality arrays");
    t.check(doc.issues().isEmpty() && doc.data.value("steps").toArray().first().toObject().value("originalQualities") == QJsonArray{"red", "dark", "unknown"}, "refresh never rewrites step conditions or incomplete draft fields");
    t.check(doc.data.value("id") == originalId && doc.data.value("revision") == originalRevision && doc.data.value("status") == "draft", "capability refresh neither publishes nor revises identity");
    doc.data.insert("steps", QJsonArray{}); doc.dirty = false; doc.refreshRequirements();
    t.check(doc.data.value("requires").toArray().contains("mouse.click.v1") && !doc.dirty, "unused declarations retained rather than silently pruned");
    doc.data.insert("requires", "invalid declaration"); const auto invalid = doc.data; doc.refreshRequirements();
    t.check(doc.data == invalid && !doc.dirty, "malformed capability declaration is not normalized away");
    t.invalid(doc.data, "requires");
    doc.data = runnable(); doc.data.insert("requires", QJsonArray{});
    t.check(doc.issues().isEmpty(), "missing declarations remain a structural draft");
    t.invalid(doc.data, "missing capability: mouse.click.v1", true);
    t.invalid(doc.data, "missing capability: check.pixel.v1", true);
    t.invalid(doc.data, "missing capability: target.hover_item.v1", true);
    doc.refreshRequirements(); t.check(doc.issues(true).isEmpty(), "refresh satisfies static capability prerequisites");
    auto requirements = doc.data.value("requires").toArray(); requirements.append("future.extension.v2"); doc.data.insert("requires", requirements); doc.refreshRequirements();
    t.invalid(doc.data, "unsupported capability", true);
    auto templateDoc = WorkflowDocument::createTransmuteDraft(); const auto templateData = templateDoc.data; templateDoc.dirty = false; templateDoc.refreshRequirements();
    t.check(templateDoc.data == templateData && !templateDoc.dirty, "built-in template declares all derived capabilities already");
}

void persistenceTests(Tests &t, const QString &root) {
    QDir dir(root); t.check(dir.mkpath("persistence"), "persistence directory");
    dir.cd("persistence");
    const QString path = dir.filePath("中文 workflow.d4workflow.json");
    auto doc = WorkflowDocument::createDraft();
    QString error = "stale error";
    t.check(doc.dirty && doc.filePath.isEmpty(), "new document stays in memory");
    t.check(doc.data.value("id") != WorkflowDocument::createDraft().data.value("id"), "document UUIDs are unique");
    t.check(doc.saveDraft(path, &error) && error.isEmpty(), "first atomic save succeeds and clears stale error");
    t.check(!doc.dirty && doc.data.value("revision") == 1 && doc.filePath == QFileInfo(path).canonicalFilePath(), "commit updates document state");
    t.check(!QFileInfo::exists(path + ".lock"), "save releases transient lock");
    WorkflowDocument reopened;
    t.check(reopened.load(path, &error) && reopened.data == doc.data && !reopened.dirty, "UTF-8 JSON and filename roundtrip");
    doc.data.insert("futureMetadata", QJsonObject{{"preserve", true}, {"array", QJsonArray{1, "text", QJsonValue::Null}}});
    editItem(doc.data, "points", 0, [](auto &p) { p.insert("futurePoint", QJsonObject{{"nested", true}}); });
    doc.setName("修改名字"); doc.data.insert("status", "ready");
    t.check(doc.saveDraft(path, &error), "saving loaded path needs no overwrite confirmation");
    t.check(doc.data.value("revision") == 2 && doc.data.value("status") == "draft" && !doc.dirty, "successful save always drafts and increments revision");
    t.check(reopened.load(path, &error) && reopened.data == doc.data, "unknown nested fields survive save/load exactly");
    const QString copyPath = dir.filePath("copy.json");
    auto fresh = WorkflowDocument::createDraft(); const auto freshState = fresh.data;
    const auto savedBytes = t.read(path);
    t.check(!fresh.saveDraft(path, &error) && fresh.data == freshState && fresh.dirty && fresh.filePath.isEmpty(), "new document cannot overwrite existing target without confirmation");
    t.check(t.read(path) == savedBytes, "refused overwrite preserves existing bytes");
    auto copy = doc; t.check(copy.saveDraft(copyPath, &error), "save as to new path succeeds");
    doc.setName("different existing target"); auto before = doc.data; const auto oldPath = doc.filePath;
    t.check(!doc.saveDraft(copyPath, &error) && doc.data == before && doc.filePath == oldPath && doc.dirty, "Save As existing file requires explicit confirmation");
    t.check(doc.saveDraft(copyPath, &error, true), "explicit overwrite allows a different existing target");
    t.check(doc.filePath == QFileInfo(copyPath).canonicalFilePath() && doc.data.value("revision") == 3, "confirmed Save As adopts destination baseline");
    auto external = doc.data; external.insert("name", "external mutation"); t.write(copyPath, external);
    doc.setName("local pending change"); before = doc.data; const auto dirtyBefore = doc.dirty;
    t.check(!doc.saveDraft(copyPath, &error), "hash detects external modification without revision change");
    t.check(!doc.saveDraft(copyPath, &error, true), "overwrite flag cannot bypass loaded-path conflict");
    t.check(doc.data == before && doc.dirty == dirtyBefore && doc.filePath == QFileInfo(copyPath).canonicalFilePath(), "conflict leaves data, revision, path and dirty unchanged");
    t.check(reopened.load(copyPath, &error) && reopened.data == external, "external edit is not overwritten");
    t.check(QFile::remove(copyPath), "delete external fixture");
    t.check(!doc.saveDraft(copyPath, &error) && !QFileInfo::exists(copyPath) && doc.data == before && doc.dirty, "external deletion cannot silently recreate loaded file");
    t.check(!doc.saveDraft(copyPath, &error, true), "explicit overwrite does not bypass external deletion");
    const QString recovered = dir.filePath("recovered.json");
    t.check(doc.saveDraft(recovered, &error), "conflicted document may save to new path");
    auto staleCopy = doc; doc.setName("first writer"); t.check(doc.saveDraft(recovered, &error), "first copied writer commits");
    staleCopy.setName("stale writer"); t.check(!staleCopy.saveDraft(recovered, &error), "copyable snapshots retain optimistic concurrency baseline");
    doc.setName("pending under lock"); before = doc.data;
    {
        QLockFile lock(QFileInfo(recovered).canonicalFilePath() + ".lock"); lock.setStaleLockTime(0);
        t.check(lock.tryLock(), "hold external save lock");
        t.check(!doc.saveDraft(recovered, &error) && doc.data == before && doc.dirty, "live lock blocks save without state mutation");
        WorkflowDocument reader; t.check(reader.load(recovered, &error), "load is read-only and does not acquire a write lock");
    }
    t.check(doc.saveDraft(recovered, &error), "save retries after lock release");
    QProcess holder;
    holder.start(QCoreApplication::applicationFilePath(), {"--hold-lock", QFileInfo(recovered).canonicalFilePath()});
    t.check(holder.waitForStarted(3000), "start independent lock-holder process");
    t.check(holder.waitForReadyRead(3000) && holder.readLine().trimmed() == "locked", "independent process acquired lock");
    before = doc.data; t.check(!doc.saveDraft(recovered, &error) && doc.data == before && !doc.dirty, "cross-process lock refuses even clean save");
    holder.write("release\n"); holder.closeWriteChannel();
    t.check(holder.waitForFinished(3000) && holder.exitCode() == 0, "independent lock holder exits cleanly");
    t.check(doc.saveDraft(recovered, &error), "no persistent document lock remains");

    const QString broken = dir.filePath("broken.json");
    doc.setName("keep unsaved edits"); before = doc.data; const auto baselinePath = doc.filePath;
    for (const QByteArray bytes : {QByteArray("{broken"), QByteArray("[]"), QByteArray("null"), QByteArray("{}"), QByteArray("{\"kind\":\"other\"}")}) {
        t.write(broken, bytes); t.check(!doc.load(broken, &error) && !error.isEmpty(), "parse/structure error rejects file");
        t.check(doc.data == before && doc.filePath == baselinePath && doc.dirty, "failed load preserves unsaved session");
    }
    t.write(broken, QByteArray(2 * 1024 * 1024 + 1, ' '));
    t.check(!doc.load(broken, &error) && doc.data == before, "oversized JSON rejected before parse");
    t.check(doc.saveDraft(recovered, &error), "failed load preserves original hash baseline");
    const QString invalidPath = dir.filePath("invalid-save.json");
    doc.data.insert("steps", QJsonArray{QJsonObject{{"id", "bad"}, {"name", "bad"}, {"action", "shell"}}}); doc.dirty = true; before = doc.data;
    t.check(!doc.saveDraft(invalidPath, &error) && doc.data == before && doc.dirty && !QFileInfo::exists(invalidPath), "structural validation blocks save before creating target");
    t.check(doc.load(recovered, &error), "restore structurally valid session");
    doc.data.insert("revision", std::numeric_limits<int>::max()); doc.dirty = true; before = doc.data;
    t.check(!doc.saveDraft(invalidPath, &error) && doc.data == before && doc.dirty, "revision overflow cannot mutate or persist document");
    t.check(doc.load(recovered, &error), "restore revision baseline");
    doc.data.insert("futurePayload", QString(2 * 1024 * 1024, 'x')); doc.dirty = true; before = doc.data;
    t.check(!doc.saveDraft(invalidPath, &error) && doc.data == before && doc.dirty && !QFileInfo::exists(invalidPath), "serialization size limit preserves state and disk");
    t.check(doc.load(recovered, &error), "restore after oversized save");
    doc.setName("I/O failure pending"); doc.data.insert("status", "ready"); before = doc.data;
    t.check(!doc.saveDraft(dir.filePath("missing/child.json"), &error), "missing parent directory fails save");
    t.check(!doc.saveDraft(QString{}, &error), "empty destination fails save");
    t.check(!doc.saveDraft(dir.path(), &error, true), "directory is never overwritten");
    t.check(doc.data == before && doc.dirty && doc.filePath == baselinePath, "I/O failures preserve ready status, revision, dirty flag and loaded path");

#ifdef Q_OS_UNIX
    // Named atomic temporaries need a longer basename than the lock. On file
    // systems with NAME_MAX=255 this can exercise QSaveFile::open failure after
    // a successful lock, without filling the user's disk or using fault hooks.
    bool exercisedAtomicFailure = false;
    for (int length = 248; length <= 250 && !exercisedAtomicFailure; ++length) {
        const QString tooLongForTemporary = dir.filePath(QString(length, 'n'));
        QLockFile probeLock(tooLongForTemporary + ".lock");
        if (!probeLock.tryLock()) continue;
        QSaveFile probe(tooLongForTemporary); probe.setDirectWriteFallback(false);
        if (probe.open(QIODevice::WriteOnly)) { probe.cancelWriting(); continue; }
        probeLock.unlock();
        const auto beforeBytes = t.read(recovered);
        t.check(!doc.saveDraft(tooLongForTemporary, &error), "atomic temporary creation failure returns an error after lock acquisition");
        t.check(doc.data == before && doc.dirty && doc.filePath == baselinePath && t.read(recovered) == beforeBytes && !QFileInfo::exists(tooLongForTemporary), "QSaveFile failure preserves JSON, revision, dirty, baseline and original disk bytes");
        t.check(!QFileInfo::exists(tooLongForTemporary + ".lock"), "QSaveFile failure releases lock");
        exercisedAtomicFailure = true;
    }
    if (!exercisedAtomicFailure) QTextStream(stdout) << "SKIP: filesystem supports unnamed/truncated atomic temporary names\n";
#endif

#ifndef Q_OS_WIN
    // Symlinks are tested only where QFile::link creates an actual symlink.
    const QString alias = dir.filePath("alias.json"); t.check(QFile::link(recovered, alias), "create file symlink");
    WorkflowDocument linked; t.check(linked.load(alias, &error) && linked.filePath == QFileInfo(recovered).canonicalFilePath(), "load resolves file symlink to canonical path");
    {
        QLockFile lock(QFileInfo(recovered).canonicalFilePath() + ".lock"); t.check(lock.tryLock(), "lock canonical file");
        t.check(!linked.saveDraft(alias, &error), "file alias cannot evade canonical lock");
    }
    linked.setName("saved through alias"); t.check(linked.saveDraft(alias, &error) && QFileInfo(alias).isSymLink(), "saving alias atomically updates real file without destroying symlink");
    const QString dirAlias = QDir(root).filePath("persistence-alias"); t.check(QFile::link(dir.path(), dirAlias), "create parent directory alias");
    const QString newAliasPath = QDir(dirAlias).filePath("new.json"), canonicalNew = dir.filePath("new.json");
    {
        QLockFile lock(canonicalNew + ".lock"); t.check(lock.tryLock(), "lock new canonical path");
        auto newDocument = WorkflowDocument::createDraft(); t.check(!newDocument.saveDraft(newAliasPath, &error) && !QFileInfo::exists(canonicalNew), "directory alias cannot evade lock for a new file");
    }
    auto throughDirectory = WorkflowDocument::createDraft();
    t.check(throughDirectory.saveDraft(newAliasPath, &error) && throughDirectory.filePath == QFileInfo(canonicalNew).canonicalFilePath(), "new file via directory alias adopts canonical path");
    const QString dangling = dir.filePath("dangling.json"); t.check(QFile::link(dir.filePath("absent.json"), dangling), "create dangling symlink");
    before = throughDirectory.data; t.check(!throughDirectory.saveDraft(dangling, &error, true) && QFileInfo(dangling).isSymLink() && throughDirectory.data == before, "dangling symlinks are never replaced");
    const auto permissions = QFileInfo(canonicalNew).permissions();
    t.check(QFile::setPermissions(canonicalNew, QFileDevice::ReadOwner | QFileDevice::ReadGroup | QFileDevice::ReadOther), "make destination read-only");
    if (!QFileInfo(canonicalNew).isWritable()) {
        throughDirectory.setName("read-only edit"); before = throughDirectory.data;
        t.check(!throughDirectory.saveDraft(canonicalNew, &error) && throughDirectory.data == before && throughDirectory.dirty, "read-only save cannot clear pending changes");
    }
    t.check(QFile::setPermissions(canonicalNew, permissions), "restore destination permissions");
#endif
    const QString caseAlias = dir.filePath("RECOVERED.JSON");
    if (QFileInfo::exists(caseAlias)) {
        WorkflowDocument caseSession;
        t.check(caseSession.load(recovered, &error), "load case-insensitive file fixture");
        caseSession.setName("case alias save");
        const bool savedAlias = caseSession.saveDraft(caseAlias, &error);
        t.check(savedAlias, "case alias of loaded file does not require overwrite: " + error);
        // Some platforms normalize casing in canonicalFilePath(), others retain
        // the caller's spelling. Exercise the latter even on a normalizing host.
        caseSession.filePath = caseAlias;
        caseSession.setName("save with differently cased loaded spelling");
        const bool savedSpelling = caseSession.saveDraft(recovered, &error);
        t.check(savedSpelling, "loaded spelling alias identifies the same file: " + error);
        const auto lockedState = caseSession.data;
        {
            QLockFile lock(recovered + ".lock");
            t.check(lock.tryLock(), "lock lowercase path before alias save");
            t.check(!caseSession.saveDraft(caseAlias, &error) && caseSession.data == lockedState,
                    "case alias cannot evade the existing save lock");
        }
        auto changed = caseSession.data; changed.insert("name", "external case edit"); t.write(recovered, changed);
        caseSession.filePath = caseAlias;
        const auto pending = caseSession.data;
        t.check(!caseSession.saveDraft(recovered, &error, true) && caseSession.data == pending, "case alias must not bypass loaded-file conflict with overwrite flag");
        t.check(t.read(recovered) == QJsonDocument(changed).toJson(), "alias conflict preserves external bytes");
        t.check(caseSession.load(recovered, &error), "reload before alias deletion test");
        caseSession.filePath = caseAlias;
        caseSession.setName("pending after alias deletion");
        const auto beforeDeletion = caseSession.data;
        t.check(QFile::remove(recovered), "delete case-insensitive fixture externally");
        t.check(!caseSession.saveDraft(recovered, &error, true) && caseSession.data == beforeDeletion
                    && caseSession.dirty && !QFileInfo::exists(recovered),
                "case alias cannot recreate an externally deleted loaded file");
    } else {
        // Case-sensitive APFS/Linux/NTFS directories can contain BOTH spellings.
        auto distinct = WorkflowDocument::createDraft();
        t.check(distinct.saveDraft(caseAlias, &error), "create distinct uppercase file");
        WorkflowDocument caseSession;
        t.check(caseSession.load(recovered, &error), "load distinct lowercase file");
        const auto originalBytes = t.read(recovered), otherBytes = t.read(caseAlias);
        t.check(!caseSession.saveDraft(caseAlias, &error), "distinct case-sensitive file still requires overwrite confirmation");
        t.check(t.read(recovered) == originalBytes && t.read(caseAlias) == otherBytes,
                "case-sensitive overwrite refusal preserves both files");
        t.check(caseSession.saveDraft(caseAlias, &error, true), "explicit overwrite allows distinct case-sensitive file");
    }
}

void catalogAndTemplateTests(Tests &t, const QString &root) {
    t.check(WorkflowDocument::catalog(QString{}).isEmpty(), "pathless new session never scans CWD");
    QDir dir(root); t.check(dir.mkpath("catalog"), "catalog directory"); dir.cd("catalog");
    t.check(WorkflowDocument::catalog(dir.path()).isEmpty(), "empty directory has empty catalog");
    QString error;
    for (const auto &name : {"b 中文.json", "a_lower.json", "A_upper.JSON", "Å.json", "z.json"}) {
        auto doc = WorkflowDocument::createDraft(); t.check(doc.saveDraft(dir.filePath(name), &error), "create catalog fixture " + QString(name));
    }
    t.write(dir.filePath("unrelated.json"), QJsonObject{{"hello", true}});
    t.write(dir.filePath("broken.d4workflow.json"), QByteArray("{bad"));
    auto invalid = WorkflowDocument::createDraft().data; invalid.insert("schemaVersion", 2); t.write(dir.filePath("owned.json"), invalid);
    QStringList errors; auto entries = WorkflowDocument::catalog(dir.path(), &errors);
    t.check(errors.size() == 2, "damaged owned documents reported; unrelated JSON ignored");
    t.check(!entries.isEmpty() && QFileInfo(entries.first()).fileName().startsWith('A', Qt::CaseInsensitive), "catalog orders normalized case-insensitive filenames");
    auto duplicated = WorkflowDocument::createDraft().data;
    t.write(dir.filePath("duplicate1.json"), duplicated); t.write(dir.filePath("duplicate2.json"), duplicated);
    errors.clear(); entries = WorkflowDocument::catalog(dir.path(), &errors);
    t.check(errors.size() == 4 && !entries.contains(dir.filePath("duplicate1.json")) && !entries.contains(dir.filePath("duplicate2.json")), "duplicate workflow UUIDs are reported and excluded from arbitrary selection");

    auto doc = WorkflowDocument::createTransmuteDraft();
    t.check(doc.dirty && doc.filePath.isEmpty() && doc.data.value("revision") == 0 && doc.data.value("status") == "draft", "transmute template creates a new unsaved draft");
    t.check(doc.issues().isEmpty() && !doc.issues(true).isEmpty(), "transmute draft is structural but never run-ready");
    t.check(doc.data.value("id") != WorkflowDocument::createTransmuteDraft().data.value("id"), "template sessions get fresh UUIDs");
    const auto steps = doc.data.value("steps").toArray();
    t.check(steps.size() == 14, "template keeps all 13 INI steps plus explicit load");
    const auto load = steps.first().toObject();
    t.check(load.value("button") == "right" && load.value("target").toObject().value("type") == "current_item" && load.value("minDelayMs") == 250, "load matches actual runner right-click and 250ms delay");
    t.check(doc.data.value("source").toObject().value("mode") == "inventory_once" && doc.data.value("source").toObject().value("maxItems") == 33, "template preserves inventory source and bounded count");
    t.check(doc.data.value("source").toObject().value("originalQualities") == QJsonArray{"red"}, "legacy optional yellow branch does not change red-only default");
    t.check(doc.data.value("limits").toObject().value("totalTimeoutMs") == 300000, "template preserves source run timeout");
    t.check(doc.layout().value("referenceClientPx").isNull() && doc.layout().value("inventory").toObject().value("rect").isNull(), "template never guesses reference or inventory calibration");
    for (const auto &position : doc.layout().value("pointPositions").toObject()) t.check(position.isNull(), "all template positions, including confirmation, remain uncalibrated");
    const QDir source(QFileInfo(QString::fromUtf8(__FILE__)).absolutePath());
    const QString iniPath = source.absoluteFilePath("../../ahk/config/cube-workflows.ini");
    const auto ini = t.read(iniPath);
    const auto legacy = doc.data.value("provenance").toObject().value("legacySteps").toArray();
    int checked = 0;
    for (const auto &line : ini.split('\n')) {
        const auto trimmed = line.trimmed();
        if (!trimmed.startsWith("Step") || trimmed.startsWith("StepCount")) continue;
        const int equals = trimmed.indexOf('='); if (equals < 0) continue;
        const auto parts = trimmed.mid(equals + 1).split('|');
        if (parts.size() != 9 || checked >= legacy.size()) { t.check(false, "INI step parsing"); continue; }
        const auto old = legacy.at(checked).toObject(), step = steps.at(checked + 1).toObject();
        t.check(old.value("label").toString() == QString::fromUtf8(parts[0]) && old.value("action").toString() == QString::fromUtf8(parts[1]) && old.value("quality").toString() == QString::fromUtf8(parts[2]), "template preserves source names/actions/qualities in provenance");
        t.check(old.value("x").toDouble() == parts[3].toDouble() && old.value("y").toDouble() == parts[4].toDouble(), "source coordinates preserved only as provenance");
        t.check(step.value("minDelayMs").toInt() == parts[5].toInt() && step.value("timeoutMs").toInt() == parts[8].toInt(), "template step timing equals INI");
        t.check(old.value("color").toString() == QString::fromUtf8(parts[6]) && old.value("tolerance").toInt() == parts[7].toInt(), "pending check provenance retained");
        const QJsonArray qualities = parts[2] == "any" ? QJsonArray{} : QJsonArray{QString::fromUtf8(parts[2])};
        t.check(step.value("originalQualities") == qualities && step.value("completionCheckId").isNull(), "original-quality route retained; completion never fabricated");
        ++checked;
    }
    t.check(checked == 13, "all actual INI recipe steps compared");
    const QString templatePath = source.absoluteFilePath("../templates/auto-transmute.d4workflow.json");
    WorkflowDocument templateDocument;
    t.check(templateDocument.load(templatePath, &error), "versioned template loads: " + error);
    auto generated = doc.data; generated.insert("id", templateDocument.data.value("id"));
    t.check(generated == templateDocument.data, "template file and factory have identical JSON apart from fresh workflow UUID");
    const auto schema = QJsonDocument::fromJson(t.read(source.absoluteFilePath("../schema/workflow-v1.schema.json"))).object();
    t.check(schema.value("$defs").isObject() && schema.value("properties").isObject(), "versioned structural JSON schema is available");
}
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    if (app.arguments().size() == 2 && app.arguments().at(1) == "--print-template") {
        auto data = WorkflowDocument::createTransmuteDraft().data;
        data.insert("id", "66cdd147-5e6b-4e4f-a7b4-59daf08d3c19");
        QTextStream(stdout) << QJsonDocument(data).toJson(QJsonDocument::Indented);
        return 0;
    }
    if (app.arguments().size() == 3 && app.arguments().at(1) == "--hold-lock") {
        QLockFile lock(app.arguments().at(2) + ".lock"); lock.setStaleLockTime(0);
        if (!lock.tryLock()) return 2;
        QTextStream(stdout) << "locked\n" << Qt::flush;
        QFile input; if (!input.open(stdin, QIODevice::ReadOnly)) return 3;
        input.readLine(); return 0;
    }
    Tests tests;
    QTemporaryDir temporary;
    tests.check(temporary.isValid(), "temporary directory");
    if (!temporary.isValid()) return 1;
    validationTests(tests);
    mutationTests(tests);
    requirementTests(tests);
    persistenceTests(tests, temporary.path());
    catalogAndTemplateTests(tests, temporary.path());
    QTextStream(stdout) << tests.assertions << " assertions, " << tests.failures << " failures\n";
    return tests.failures ? 1 : 0;
}
