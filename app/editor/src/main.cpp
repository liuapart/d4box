#include "EditorWindow.h"
#include "EditorTheme.h"
#include "EditorUpdates.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLockFile>
#include <QScreen>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTextStream>
#include <QTimer>
#include <memory>

namespace {
int reportError(const QString &message, int code) {
    QTextStream(stderr) << message << '\n';
    return code;
}
int validateCommand(int argc, char **argv) {
    // Pure document validation requires neither a GUI platform plugin nor a display.
    QCoreApplication app(argc, argv);
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOptions({{{QStringLiteral("for-run")}, QStringLiteral("检查静态运行前提；不执行")},
                       {{QStringLiteral("format")},
                        QStringLiteral("human 或 json"),
                        QStringLiteral("format"),
                        QStringLiteral("human")}});
    parser.addPositionalArgument(QStringLiteral("validate"), QStringLiteral("子命令"));
    parser.addPositionalArgument(QStringLiteral("path"), QStringLiteral("配置文件或目录"));
    if (!parser.parse(app.arguments()))
        return reportError(parser.errorText(), 2);
    if (parser.isSet(QStringLiteral("help")))
        parser.showHelp();
    const auto positional = parser.positionalArguments();
    const auto format = parser.value(QStringLiteral("format"));
    if (positional.size() != 2 || (format != QStringLiteral("human") && format != QStringLiteral("json")))
        return reportError(
            QStringLiteral("用法：D4BoxEditor validate PATH [--for-run] [--format human|json]"), 2);
    QFileInfo info(positional.at(1));
    if (!info.exists())
        return reportError(QStringLiteral("配置路径不存在。"), 2);
    QStringList invalid, files;
    if (info.isDir())
        files = WorkflowDocument::catalog(info.canonicalFilePath(), &invalid);
    else
        files.append(info.canonicalFilePath());
    QJsonArray results;
    bool valid = !files.isEmpty() && invalid.isEmpty();
    QHash<QString, QString> identities;
    for (const auto &path : files) {
        WorkflowDocument doc;
        QString error;
        QStringList errors;
        if (!doc.load(path, &error))
            errors.append(error);
        else {
            errors = doc.issues(parser.isSet(QStringLiteral("for-run")));
            const auto id = doc.data.value(QStringLiteral("id")).toString();
            if (identities.contains(id))
                errors.append(QStringLiteral("配置 ID 与 %1 重复；请通过“复制配置”生成新 ID。")
                                  .arg(identities.value(id)));
            else
                identities.insert(id, QFileInfo(path).fileName());
        }
        valid &= errors.isEmpty();
        results.append(QJsonObject{{QStringLiteral("file"), QFileInfo(path).fileName()},
                                   {QStringLiteral("valid"), errors.isEmpty()},
                                   {QStringLiteral("errors"), QJsonArray::fromStringList(errors)}});
    }
    const QJsonObject output{{QStringLiteral("valid"), valid},
                             {QStringLiteral("scope"), parser.isSet(QStringLiteral("for-run"))
                                                           ? QStringLiteral("static-prerequisites-only")
                                                           : QStringLiteral("structure-only")},
                             {QStringLiteral("files"), results},
                             {QStringLiteral("catalogErrors"), QJsonArray::fromStringList(invalid)}};
    if (format == QStringLiteral("json"))
        QTextStream(stdout) << QJsonDocument(output).toJson();
    else {
        QTextStream(stdout) << (valid ? "PASS" : "FAIL") << " · static validation only; no game execution\n";
        for (const auto &result : results) {
            const auto r = result.toObject();
            QTextStream(stdout) << r.value(QStringLiteral("file")).toString() << " : "
                                << (r.value(QStringLiteral("valid")).toBool() ? "OK" : "ERROR") << '\n';
            for (const auto &e : r.value(QStringLiteral("errors")).toArray())
                QTextStream(stdout) << "  " << e.toString() << '\n';
        }
        for (const auto &e : invalid)
            QTextStream(stdout) << e << '\n';
        if (files.isEmpty())
            QTextStream(stdout) << "No valid workflows found\n";
    }
    return valid ? 0 : 3;
}
} // namespace

int main(int argc, char **argv) {
    if (argc > 1 && QByteArray(argv[1]) == "validate")
        return validateCommand(argc, argv);
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("D4BoxEditor"));
    QApplication::setApplicationVersion(QStringLiteral(D4BOX_EDITOR_VERSION));
    QApplication::setOrganizationName(QStringLiteral("D4Box"));
    EditorTheme::install(app);
    app.setQuitOnLastWindowClosed(false);
    QImageReader::setAllocationLimit(128);
    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("独立流程配置编辑器：全屏透明覆盖、局部拖柄、JSON 草稿；不发送游戏操作。"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("path"), QStringLiteral("已有配置 JSON 或目录（可选）"),
                                 QStringLiteral("[path]"));
    parser.addOptions(
        {{{QStringLiteral("new")}, QStringLiteral("新建未保存草稿，不打开目录首个配置")},
         {{QStringLiteral("template")}, QStringLiteral("用未校准的内置自动嬗变模板新建")},
         {{QStringLiteral("background")},
          QStringLiteral("本地 PNG/JPEG 测试底图，不写入配置"),
          QStringLiteral("image")},
         {{QStringLiteral("overlay")}, QStringLiteral("透明桌面覆盖（默认）；与底图互斥")},
         {{QStringLiteral("no-update-check")}, QStringLiteral("本次启动不自动检查编辑器更新")},
         {{QStringLiteral("screen")},
          QStringLiteral("0 起始的显示器索引"),
          QStringLiteral("index"),
          QStringLiteral("0")},
         {{QStringLiteral("list-screens")}, QStringLiteral("输出显示器及逻辑尺寸 JSON 后退出")},
         {{QStringLiteral("smoke-test")}, QStringLiteral("创建覆盖层和面板后退出，不写配置")},
         {{QStringLiteral("ui-self-test")}, QStringLiteral("本程序内部离线交互测试；不注入系统输入")},
         {{QStringLiteral("diagnose-overlay")}, QStringLiteral("输出原生窗口穿透标志与几何 JSON 后退出")},
         {{QStringLiteral("runtime-info")}, QStringLiteral("输出运行库/插件诊断 JSON 后退出")},
         {{QStringLiteral("render")},
          QStringLiteral("导出静态编辑预览 PNG，不捕获桌面"),
          QStringLiteral("png")}});
    if (!parser.parse(app.arguments()))
        return reportError(parser.errorText(), 2);
    if (parser.isSet(QStringLiteral("help")))
        parser.showHelp();
    if (parser.isSet(QStringLiteral("version")))
        parser.showVersion();
    if (parser.isSet(QStringLiteral("runtime-info"))) {
        QTextStream(stdout)
            << QJsonDocument(QJsonObject{{QStringLiteral("qtVersion"), QString::fromLatin1(qVersion())},
                                         {QStringLiteral("architecture"), QSysInfo::currentCpuArchitecture()},
                                         {QStringLiteral("platform"), QGuiApplication::platformName()},
                                         {QStringLiteral("pluginPaths"),
                                          QJsonArray::fromStringList(QCoreApplication::libraryPaths())}})
                   .toJson();
        return 0;
    }
    if (parser.isSet(QStringLiteral("list-screens"))) {
        QJsonArray screens;
        int i = 0;
        for (auto *s : QGuiApplication::screens())
            screens.append(QJsonObject{{QStringLiteral("index"), i++},
                                       {QStringLiteral("name"), s->name()},
                                       {QStringLiteral("x"), s->geometry().x()},
                                       {QStringLiteral("y"), s->geometry().y()},
                                       {QStringLiteral("width"), s->geometry().width()},
                                       {QStringLiteral("height"), s->geometry().height()},
                                       {QStringLiteral("dpr"), s->devicePixelRatio()}});
        QTextStream(stdout) << QJsonDocument(screens).toJson();
        return 0;
    }
    int diagnosticModes = 0;
    for (const auto &key : {QStringLiteral("smoke-test"), QStringLiteral("ui-self-test"),
                            QStringLiteral("diagnose-overlay"), QStringLiteral("render")})
        if (parser.isSet(key))
            ++diagnosticModes;
    if (diagnosticModes > 1)
        return reportError(QStringLiteral("诊断模式不可同时使用。"), 2);
    if (parser.isSet(QStringLiteral("overlay")) && parser.isSet(QStringLiteral("background")))
        return reportError(QStringLiteral("--overlay 与 --background 互斥。"), 2);
    bool screenOK;
    const int screen = parser.value(QStringLiteral("screen")).toInt(&screenOK);
    if (!screenOK || screen < 0 || screen >= QGuiApplication::screens().size())
        return reportError(QStringLiteral("显示器索引无效；使用 --list-screens 查询。"), 2);
    const auto args = parser.positionalArguments();
    if (args.size() > 1)
        return reportError(QStringLiteral("只接受一个文件或目录参数。"), 2);
    WorkflowDocument document = parser.isSet(QStringLiteral("template"))
                                    ? WorkflowDocument::createTransmuteDraft()
                                    : WorkflowDocument::createDraft();
    QString directory;
    const bool newDocument = parser.isSet(QStringLiteral("new")) || parser.isSet(QStringLiteral("template"));
    if (!args.isEmpty()) {
        QFileInfo path(args.first());
        if (!path.exists() || (!path.isDir() && !path.isFile()))
            return reportError(QStringLiteral("配置路径不存在或不是普通文件/目录。"), 2);
        if (path.isFile() && newDocument)
            return reportError(QStringLiteral("--new / --template 不能与已有文件一起使用。"), 2);
        QString toLoad;
        if (path.isDir()) {
            directory = path.canonicalFilePath();
            QStringList invalid;
            const auto files = WorkflowDocument::catalog(directory, &invalid);
            for (const auto &warning : invalid)
                QTextStream(stderr) << warning << '\n';
            if (!newDocument) {
                if (files.isEmpty() && !invalid.isEmpty())
                    return reportError(QStringLiteral("目录内没有可加载的配置；可用 --new 显式新建。"), 3);
                if (!files.isEmpty())
                    toLoad = files.first();
            }
        } else {
            toLoad = path.canonicalFilePath();
            directory = QFileInfo(toLoad).absolutePath();
        }
        QString error;
        if (!toLoad.isEmpty() && !document.load(toLoad, &error))
            return reportError(error, 3);
    }
    QImage background;
    if (parser.isSet(QStringLiteral("background"))) {
        QImageReader reader(parser.value(QStringLiteral("background")));
        reader.setAutoTransform(true);
        background = reader.read();
        if (background.isNull())
            return reportError(QStringLiteral("无法读取底图：") + reader.errorString(), 4);
    }
    const auto platform = QGuiApplication::platformName();
    if (!diagnosticModes && background.isNull() && platform != QStringLiteral("cocoa") &&
        platform != QStringLiteral("windows"))
        return reportError(QStringLiteral("交互覆盖层只支持 Cocoa/macOS 和 Windows；其他后端仅供离线诊断。"),
                           5);
    std::unique_ptr<QLockFile> sessionLock;
    if (!diagnosticModes) {
        const auto lockDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        if (!QDir().mkpath(lockDir))
            return reportError(QStringLiteral("无法创建编辑会话锁目录。"), 4);
        sessionLock =
            std::make_unique<QLockFile>(QDir(lockDir).filePath(QStringLiteral("screen-%1.lock").arg(screen)));
        if (!sessionLock->tryLock())
            return reportError(QStringLiteral("此显示器已有编辑会话，请先退出另一个编辑器。"), 5);
    }
    EditorWindow window(std::move(document), directory, background, screen);
    window.start();
    if (!diagnosticModes && background.isNull() && !parser.isSet(QStringLiteral("no-update-check")) &&
        EditorUpdates::isPackaged())
        QTimer::singleShot(1200, &window, [&window] { window.checkUpdates(false); });
    if (parser.isSet(QStringLiteral("smoke-test"))) {
        QString resourceError;
        if (!EditorTheme::validateResources(&resourceError))
            return reportError(resourceError, 6);
        if (QIcon(QStringLiteral(":/brand/app-icon.svg")).pixmap(64, 64).isNull())
            return reportError(QStringLiteral("编辑器图标资源缺失。"), 6);
        window.showSteps();
        QTimer::singleShot(250, &window, &EditorWindow::closePreview);
    }
    if (parser.isSet(QStringLiteral("diagnose-overlay")))
        QTimer::singleShot(250, &app, [&] {
            QTextStream(stdout) << QJsonDocument(window.diagnostics()).toJson();
            window.closePreview();
        });
    if (parser.isSet(QStringLiteral("ui-self-test")))
        QTimer::singleShot(250, &app, [&] {
            const auto result = window.selfTest();
            QTextStream(stdout) << QJsonDocument(result).toJson();
            const bool failed = !result.value(QStringLiteral("failed")).toArray().isEmpty();
            window.closePreview();
            if (failed)
                QTimer::singleShot(0, &app, [&app] { app.exit(6); });
        });
    if (parser.isSet(QStringLiteral("render"))) {
        const auto output = parser.value(QStringLiteral("render"));
        QTimer::singleShot(300, &app, [&window, output, &app] {
            if (!window.renderPreview().save(output, "PNG")) {
                QTextStream(stderr) << "Unable to save preview\n";
                window.closePreview();
                QTimer::singleShot(0, &app, [&app] { app.exit(4); });
            } else
                window.closePreview();
        });
    }
    return app.exec();
}
