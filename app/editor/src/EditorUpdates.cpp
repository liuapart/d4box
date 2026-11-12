#include "EditorUpdates.h"
#include "EditorDialogs.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QTimer>
#include <QVersionNumber>
#include <QVBoxLayout>

namespace {
constexpr auto apiUrl = "https://api.github.com/repos/liuapart/d4box/releases?per_page=100";
constexpr auto releasePrefix = "https://github.com/liuapart/d4box/releases/download/";
constexpr qint64 maximumPackageBytes = 600LL * 1024 * 1024;

QString downloadUrl(const QString &tag, const QString &name) {
    return QString::fromLatin1(releasePrefix) + tag + QLatin1Char('/') + name;
}
QString curlExecutable() {
#ifdef Q_OS_MACOS
    return QStringLiteral("/usr/bin/curl");
#elif defined(Q_OS_WIN)
    return QDir(qEnvironmentVariable("SystemRoot")).filePath(QStringLiteral("System32/curl.exe"));
#else
    return QStandardPaths::findExecutable(QStringLiteral("curl"));
#endif
}
QString installRoot() {
#ifdef Q_OS_MACOS
    return QFileInfo(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("../..")))
        .canonicalFilePath();
#else
    return QCoreApplication::applicationDirPath();
#endif
}

class UpdateProgress final : public EditorDialog {
public:
    explicit UpdateProgress(QWidget *parent) : EditorDialog(parent, QStringLiteral("更新编辑器")) {
        resize(440, 150);
        label = new QLabel(QStringLiteral("正在准备更新…"));
        label->setWordWrap(true);
        body->addWidget(label);
        auto *bar = new QProgressBar;
        bar->setRange(0, 0);
        bar->setTextVisible(false);
        body->addWidget(bar);
        auto *cancel = new QPushButton(QStringLiteral("取消"));
        body->addWidget(cancel, 0, Qt::AlignRight);
        connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    }
    QLabel *label;
};
} // namespace

QString EditorUpdates::assetName(const QString &version, const QString &platform,
                                 const QString &architecture, bool installed) {
    const auto stem = QStringLiteral("D4BoxEditor-%1-").arg(version);
    if (platform == QStringLiteral("windows") && architecture == QStringLiteral("x86_64"))
        return stem + (installed ? QStringLiteral("windows-x64-setup.exe")
                                 : QStringLiteral("windows-x64.zip"));
    if (platform == QStringLiteral("cocoa")) {
        if (architecture == QStringLiteral("arm64")) return stem + QStringLiteral("macos-arm64.zip");
        if (architecture == QStringLiteral("x86_64")) return stem + QStringLiteral("macos-x64.zip");
    }
    return {};
}

std::optional<EditorRelease> EditorUpdates::newest(const QByteArray &json, const QString &currentVersion,
                                                   const QString &asset, QString *error) {
    if (error) error->clear();
    QJsonParseError parseError;
    const auto parsed = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !parsed.isArray()) {
        if (error) *error = QStringLiteral("GitHub 发布列表格式无效：") + parseError.errorString();
        return std::nullopt;
    }
    const QRegularExpression tagPattern(QStringLiteral("^editor-v(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)$"));
    const auto current = QVersionNumber::fromString(currentVersion);
    QVersionNumber best = current;
    const bool currentIsPrerelease = currentVersion.contains(QLatin1Char('-'));
    std::optional<EditorRelease> result;
    for (const auto &entry : parsed.array()) {
        const auto object = entry.toObject();
        if (object.value(QStringLiteral("draft")).toBool() ||
            object.value(QStringLiteral("prerelease")).toBool()) continue;
        const auto tag = object.value(QStringLiteral("tag_name")).toString();
        const auto match = tagPattern.match(tag);
        if (!match.hasMatch()) continue;
        const auto version = tag.mid(8);
        const auto parsedVersion = QVersionNumber::fromString(version);
        const int comparison = QVersionNumber::compare(parsedVersion, best);
        if (parsedVersion.segmentCount() != 3 || comparison < 0 ||
            (comparison == 0 && (!currentIsPrerelease || result)))
            continue;
        QString packageUrl, sumsUrl;
        for (const auto &value : object.value(QStringLiteral("assets")).toArray()) {
            const auto item = value.toObject();
            const auto name = item.value(QStringLiteral("name")).toString();
            const auto url = item.value(QStringLiteral("browser_download_url")).toString();
            if (url != downloadUrl(tag, name)) continue;
            if (name == asset.arg(version)) packageUrl = url;
            if (name == QStringLiteral("SHA256SUMS.txt")) sumsUrl = url;
        }
        if (packageUrl.isEmpty() || sumsUrl.isEmpty()) continue;
        best = parsedVersion;
        result = EditorRelease{version, asset.arg(version), packageUrl, sumsUrl};
    }
    return result;
}

QByteArray EditorUpdates::expectedHash(const QByteArray &checksums, const QString &asset) {
    if (checksums.size() > 128 * 1024) return {};
    for (const auto &line : checksums.split('\n')) {
        const auto trimmed = line.trimmed();
        if (trimmed.size() != 66 + asset.size()) continue;
        if (trimmed.at(64) != ' ' || (trimmed.at(65) != ' ' && trimmed.at(65) != '*')) continue;
        if (trimmed.mid(66) != asset.toUtf8()) continue;
        const auto hash = trimmed.left(64).toLower();
        if (QRegularExpression(QStringLiteral("^[0-9a-f]{64}$")).match(QString::fromLatin1(hash)).hasMatch())
            return hash;
    }
    return {};
}

QStringList EditorUpdates::publicDownloadArgs(const QString &url, const QString &outputPath,
                                               qint64 maximumBytes, int timeoutSeconds) {
    return {QStringLiteral("--disable"), QStringLiteral("--location"),
            QStringLiteral("--fail"), QStringLiteral("--silent"),
            QStringLiteral("--show-error"), QStringLiteral("--proto"), QStringLiteral("=https"),
            QStringLiteral("--proto-redir"), QStringLiteral("=https"),
            QStringLiteral("--header"), QStringLiteral("User-Agent: D4BoxEditor"),
            QStringLiteral("--header"), QStringLiteral("Accept: application/vnd.github+json"),
            QStringLiteral("--max-filesize"), QString::number(maximumBytes),
            QStringLiteral("--max-time"), QString::number(timeoutSeconds),
            QStringLiteral("--output"), outputPath, url};
}

bool EditorUpdates::isPackaged(QString *reason) {
    const auto root = installRoot();
#ifdef Q_OS_MACOS
    const bool packaged = root.endsWith(QStringLiteral(".app")) &&
                          QFileInfo::exists(root + QStringLiteral("/Contents/Info.plist")) &&
                          QFileInfo::exists(root + QStringLiteral("/Contents/Frameworks/QtCore.framework")) &&
                          QFileInfo::exists(root + QStringLiteral("/Contents/Resources/EDITOR-NOTICE.txt"));
#elif defined(Q_OS_WIN)
    const bool packaged = QFileInfo::exists(root + QStringLiteral("/EDITOR-NOTICE.txt")) &&
                          QFileInfo::exists(root + QStringLiteral("/Qt6Widgets.dll"));
#else
    const bool packaged = false;
#endif
    if (!packaged) {
        if (reason) *reason = QStringLiteral("开发构建不能被在线发布包覆盖；请使用已安装的发布版更新。");
        return false;
    }
    QTemporaryFile probe(QDir(QFileInfo(root).absolutePath()).filePath(QStringLiteral(".d4box-update-XXXXXX")));
    if (!probe.open()) {
        if (reason) *reason = QStringLiteral("当前应用所在目录不可写；请把应用移到可写位置后重试。");
        return false;
    }
    return true;
}

EditorUpdateController::EditorUpdateController(QWidget *widget, std::function<bool()> prepare,
        std::function<void()> exit, std::function<QString()> path)
    : QObject(widget), owner(widget), prepareExit(std::move(prepare)),
      exitAndRestart(std::move(exit)), restartPath(std::move(path)) {
    downloadTimer.setInterval(1000);
    connect(&downloadTimer, &QTimer::timeout, this, [this] {
        const auto bytes = QFileInfo(assetPath).size();
        if (!process || bytes <= 0) return;
        if (bytes > maximumPackageBytes) {
            process->setProperty("d4boxOversize", true);
            process->kill();
            return;
        }
        if (progressLabel)
            progressLabel->setText(QStringLiteral("正在下载 %1：%2 MiB")
                .arg(release->assetName).arg(double(bytes) / (1024 * 1024), 0, 'f', 1));
    });
}

void EditorUpdateController::run(const QString &program, const QStringList &args,
                                 const std::function<void(const QByteArray &)> &success,
                                 int timeoutMs) {
    auto *job = new QProcess(this);
    process = job;
    connect(job, &QProcess::finished, this, [this, job, success](int exitCode, QProcess::ExitStatus status) {
        if (process != job) return;
        process = nullptr;
        const auto output = job->readAllStandardOutput();
        const auto detail = QString::fromLocal8Bit(job->readAllStandardError()).trimmed();
        job->deleteLater();
        if (cancelled) return;
        if (job->property("d4boxTimedOut").toBool()) {
            fail(QStringLiteral("更新操作超时。"));
            return;
        }
        if (job->property("d4boxOversize").toBool()) {
            fail(QStringLiteral("更新包超过允许的大小，已取消下载。"));
            return;
        }
        if (status != QProcess::NormalExit || exitCode != 0) {
            fail(detail.isEmpty() ? QStringLiteral("命令执行失败（%1）。").arg(exitCode) : detail);
            return;
        }
        success(output);
    });
    connect(job, &QProcess::errorOccurred, this, [this, job](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || process != job) return;
        process = nullptr;
        const auto detail = job->errorString();
        job->deleteLater();
        if (!cancelled) fail(detail);
    });
    QTimer::singleShot(timeoutMs, job, [this, job] {
        if (process != job) return;
        job->setProperty("d4boxTimedOut", true);
        job->kill();
    });
    job->start(program, args);
}

void EditorUpdateController::check(bool requested) {
    if (busy) return;
    if (!requested) {
        QSettings settings;
        if (settings.value(QStringLiteral("updates/checkedDate")).toDate() == QDate::currentDate() &&
            settings.value(QStringLiteral("updates/checkedVersion")).toString() ==
                QCoreApplication::applicationVersion()) return;
    }
    manual = requested;
    cancelled = false;
    busy = true;
    const auto curl = curlExecutable();
    if (!QFileInfo(curl).isExecutable()) {
        fail(QStringLiteral("未找到系统 curl，无法访问公开的 GitHub Release。"));
        return;
    }
#ifdef Q_OS_WIN
    installed = QFileInfo::exists(installRoot() + QStringLiteral("/unins000.exe"));
    const auto system = QStringLiteral("windows");
#else
    installed = false;
    const auto system = QStringLiteral("cocoa");
#endif
    const auto architecture = QSysInfo::currentCpuArchitecture();
    const auto pattern = EditorUpdates::assetName(QStringLiteral("%1"), system, architecture, installed);
    if (pattern.isEmpty()) {
        fail(QStringLiteral("此平台或架构没有编辑器更新包。"));
        return;
    }
    run(curl, EditorUpdates::publicDownloadArgs(QString::fromLatin1(apiUrl),
        QStringLiteral("-"), 2 * 1024 * 1024, 30), [this, pattern](const QByteArray &json) {
        if (json.size() > 2 * 1024 * 1024) { fail(QStringLiteral("发布列表过大。")); return; }
        QString error;
        release = EditorUpdates::newest(json, QCoreApplication::applicationVersion(), pattern, &error);
        if (!error.isEmpty()) { fail(error); return; }
        QSettings().setValue(QStringLiteral("updates/checkedDate"), QDate::currentDate());
        QSettings().setValue(QStringLiteral("updates/checkedVersion"), QCoreApplication::applicationVersion());
        if (!release) {
            finish();
            if (manual) EditorMessageBox::information(owner, QStringLiteral("检查更新"),
                QStringLiteral("当前已是可用的最新稳定版（%1）。").arg(QCoreApplication::applicationVersion()));
            return;
        }
        if (!manual) {
            QSettings settings;
            if (settings.value(QStringLiteral("updates/notifiedVersion")).toString() == release->version) {
                finish();
                return;
            }
            settings.setValue(QStringLiteral("updates/notifiedVersion"), release->version);
        }
        QString reason;
        if (!EditorUpdates::isPackaged(&reason)) {
            const auto version = release->version;
            finish();
            EditorMessageBox::information(owner, QStringLiteral("发现编辑器更新"),
                QStringLiteral("发现 %1。%2\n发布页：https://github.com/liuapart/d4box/releases/tag/editor-v%1")
                    .arg(version, reason));
            return;
        }
        const auto answer = EditorMessageBox::question(owner, QStringLiteral("发现编辑器更新"),
            QStringLiteral("当前版本 %1，发现 %2。下载并校验后，是否安装并重启？\n"
                           "发布包尚未由开发者签名；SHA-256 只能检测传输/存储损坏，不能代替软件签名。")
                .arg(QCoreApplication::applicationVersion(), release->version));
        if (answer != EditorMessageBox::Yes) { finish(); return; }
        download();
    }, 30000);
}

void EditorUpdateController::setProgress(const QString &message) {
    if (!progress) {
        auto *dialog = new UpdateProgress(owner);
        progress = dialog;
        progressLabel = dialog->label;
        connect(dialog, &QDialog::rejected, this, &EditorUpdateController::cancel);
        dialog->show();
    }
    progressLabel->setText(message);
}

void EditorUpdateController::download() {
    const auto cache = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (!QDir().mkpath(cache)) { fail(QStringLiteral("无法创建更新缓存目录。")); return; }
    QTemporaryDir staging(QDir(cache).filePath(QStringLiteral("update-XXXXXX")));
    if (!staging.isValid()) { fail(QStringLiteral("无法创建更新临时目录。")); return; }
    staging.setAutoRemove(false);
    stageRoot = staging.path();
    assetPath = QDir(stageRoot).filePath(release->assetName);
    setProgress(QStringLiteral("正在下载校验清单…"));
    const auto sumsPath = QDir(stageRoot).filePath(QStringLiteral("SHA256SUMS.txt"));
    const auto curl = curlExecutable();
    run(curl, EditorUpdates::publicDownloadArgs(release->checksumsUrl, sumsPath, 128 * 1024, 120),
        [this, sumsPath, curl](const QByteArray &) {
        QFile sums(sumsPath);
        if (!sums.open(QIODevice::ReadOnly) || sums.size() > 128 * 1024 ||
            EditorUpdates::expectedHash(sums.readAll(), release->assetName).isEmpty()) {
            fail(QStringLiteral("校验清单无效或缺少当前平台的安装包。"));
            return;
        }
        setProgress(QStringLiteral("正在下载 %1…").arg(release->assetName));
        downloadTimer.start();
        run(curl, EditorUpdates::publicDownloadArgs(release->assetUrl, assetPath,
                maximumPackageBytes, 1200),
            [this, sumsPath](const QByteArray &) {
            downloadTimer.stop();
            QFile sums(sumsPath), asset(assetPath);
            if (!sums.open(QIODevice::ReadOnly) || !asset.open(QIODevice::ReadOnly) ||
                asset.size() < 100000 || asset.size() > maximumPackageBytes) {
                fail(QStringLiteral("下载文件不完整或大小异常。"));
                return;
            }
            QCryptographicHash hash(QCryptographicHash::Sha256);
            if (!hash.addData(&asset) || hash.result().toHex() !=
                EditorUpdates::expectedHash(sums.readAll(), release->assetName)) {
                fail(QStringLiteral("SHA-256 校验失败，已拒绝安装。"));
                return;
            }
            if (installed) ready(); else extract();
        }, 1200000);
    });
}

void EditorUpdateController::extract() {
    setProgress(QStringLiteral("正在解压并检查发布包…"));
    const auto dest = QDir(stageRoot).filePath(QStringLiteral("extracted"));
    QDir().mkpath(dest);
#ifdef Q_OS_MACOS
    run(QStringLiteral("/usr/bin/ditto"), {QStringLiteral("-x"), QStringLiteral("-k"), assetPath, dest},
        [this](const QByteArray &) { verifyExtracted(); });
#else
    const auto script = QDir(stageRoot).filePath(QStringLiteral("unpack.ps1"));
    QFile source(QStringLiteral(":/update/unpack-windows.ps1"));
    if (!source.open(QIODevice::ReadOnly)) { fail(QStringLiteral("缺少解压辅助程序。")); return; }
    QFile output(script);
    if (!output.open(QIODevice::WriteOnly) || output.write(source.readAll()) < 0) {
        fail(QStringLiteral("无法写入解压辅助程序。")); return;
    }
    output.close();
    run(QStringLiteral("powershell.exe"), {QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"),
        QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"), QStringLiteral("-File"), script,
        QStringLiteral("-Archive"), assetPath, QStringLiteral("-Destination"), dest},
        [this](const QByteArray &) { verifyExtracted(); });
#endif
}

void EditorUpdateController::verifyExtracted() {
#ifdef Q_OS_MACOS
    const auto app = QDir(stageRoot).filePath(QStringLiteral("extracted/D4BoxEditor.app"));
    const auto executable = QDir(app).filePath(QStringLiteral("Contents/MacOS/D4BoxEditor"));
    if (!QFileInfo::exists(executable)) { fail(QStringLiteral("发布包缺少应用程序。")); return; }
    run(QStringLiteral("/usr/bin/codesign"), {QStringLiteral("--verify"), QStringLiteral("--deep"),
        QStringLiteral("--strict"), app}, [this, executable](const QByteArray &) {
        run(executable, {QStringLiteral("--version")}, [this, executable](const QByteArray &output) {
            if (QString::fromUtf8(output).trimmed() != QStringLiteral("D4BoxEditor %1").arg(release->version)) {
                fail(QStringLiteral("下载包中的程序版本与发布信息不符。")); return;
            }
            ready();
        });
    });
#else
    const auto executable = QDir(stageRoot).filePath(QStringLiteral("extracted/D4BoxEditor/D4BoxEditor.exe"));
    if (!QFileInfo::exists(executable) ||
        !QFileInfo::exists(QFileInfo(executable).absolutePath() + QStringLiteral("/Qt6Widgets.dll"))) {
        fail(QStringLiteral("便携包内容不完整。")); return;
    }
    run(executable, {QStringLiteral("--version")}, [this](const QByteArray &output) {
        if (QString::fromUtf8(output).trimmed() != QStringLiteral("D4BoxEditor %1").arg(release->version)) {
            fail(QStringLiteral("下载包中的程序版本与发布信息不符。")); return;
        }
        ready();
    });
#endif
}

void EditorUpdateController::ready() {
    if (progress) { progress->hide(); progress->deleteLater(); progress = nullptr; progressLabel = nullptr; }
    const auto answer = EditorMessageBox::question(owner, QStringLiteral("安装更新"),
        QStringLiteral("%1 已下载并通过校验。现在退出编辑器、安装并重启？\n"
                       "请确认当前工作已保存；安装失败时旧版本会保留在旁边的备份目录。")
            .arg(release->version));
    if (answer != EditorMessageBox::Yes) { finish(); return; }
    if (!prepareExit()) { finish(); return; }
    QString error;
    if (!launchReplacement(&error)) { fail(error); return; }
    // The detached helper waits for this process to exit before touching binaries.
    finish();
    exitAndRestart();
}

bool EditorUpdateController::launchReplacement(QString *error) {
#ifdef Q_OS_MACOS
    const auto resource = QStringLiteral(":/update/replace-macos.sh");
    const auto helper = QDir(stageRoot).filePath(QStringLiteral("replace.sh"));
    const auto command = QStringLiteral("/bin/sh");
    const QStringList args{helper, QString::number(QCoreApplication::applicationPid()), installRoot(),
        QDir(stageRoot).filePath(QStringLiteral("extracted/D4BoxEditor.app")), restartPath()};
#else
    const auto resource = QStringLiteral(":/update/replace-windows.ps1");
    const auto helper = QDir(stageRoot).filePath(QStringLiteral("replace.ps1"));
    const auto command = QStringLiteral("powershell.exe");
    const QStringList args{QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"),
        QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"), QStringLiteral("-File"), helper,
        QStringLiteral("-PidToWait"), QString::number(QCoreApplication::applicationPid()),
        QStringLiteral("-InstallRoot"), installRoot(), QStringLiteral("-Payload"),
        installed ? assetPath : QDir(stageRoot).filePath(QStringLiteral("extracted/D4BoxEditor")),
        QStringLiteral("-Installed"), installed ? QStringLiteral("1") : QStringLiteral("0"),
        QStringLiteral("-ConfigPath"), restartPath()};
#endif
    QFile input(resource);
    QFile output(helper);
    if (!input.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly) ||
        output.write(input.readAll()) < 0) {
        if (error) *error = QStringLiteral("无法准备安装辅助程序。");
        return false;
    }
    output.close();
    if (!QProcess::startDetached(command, args, stageRoot)) {
        if (error) *error = QStringLiteral("无法启动安装辅助程序；编辑器未退出。");
        return false;
    }
    handedOff = true;
    return true;
}

void EditorUpdateController::fail(const QString &message) {
    const bool show = manual || progress;
    finish();
    if (show) EditorMessageBox::warning(owner, QStringLiteral("更新失败"), message);
}
void EditorUpdateController::finish() {
    busy = false;
    downloadTimer.stop();
    if (progress) { progress->hide(); progress->deleteLater(); progress = nullptr; progressLabel = nullptr; }
    if (!stageRoot.isEmpty()) {
        // A successful handoff owns this directory. It is otherwise discarded.
        if (handedOff) {
            stageRoot.clear();
        } else {
            QDir(stageRoot).removeRecursively();
            stageRoot.clear();
        }
    }
    release.reset();
    handedOff = false;
}
void EditorUpdateController::cancel() {
    cancelled = true;
    if (process) { process->kill(); process->waitForFinished(3000); }
    finish();
}
