#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QTimer>
#include <functional>
#include <optional>

class QWidget;
class QProcess;
class EditorDialog;
class QLabel;

struct EditorRelease {
    QString version;
    QString assetName;
    QString assetUrl;
    QString checksumsUrl;
};

namespace EditorUpdates {
QString assetName(const QString &version, const QString &platform, const QString &architecture,
                  bool installed = false);
std::optional<EditorRelease> newest(const QByteArray &json, const QString &currentVersion,
                                    const QString &asset, QString *error = nullptr);
QByteArray expectedHash(const QByteArray &checksums, const QString &asset);
QStringList publicDownloadArgs(const QString &url, const QString &outputPath, qint64 maximumBytes,
                               int timeoutSeconds);
bool isPackaged(QString *reason = nullptr);
}

class EditorUpdateController final : public QObject {
    Q_OBJECT
public:
    EditorUpdateController(QWidget *owner, std::function<bool()> prepareExit,
                           std::function<void()> exitAndRestart,
                           std::function<QString()> restartPath);
    void check(bool manual);
private:
    void run(const QString &program, const QStringList &args,
             const std::function<void(const QByteArray &)> &success, int timeoutMs = 120000);
    void download();
    void extract();
    void verifyExtracted();
    void ready();
    void fail(const QString &message);
    void finish();
    void cancel();
    void setProgress(const QString &message);
    bool launchReplacement(QString *error);

    QWidget *owner;
    std::function<bool()> prepareExit;
    std::function<void()> exitAndRestart;
    std::function<QString()> restartPath;
    QProcess *process = nullptr;
    EditorDialog *progress = nullptr;
    QLabel *progressLabel = nullptr;
    std::optional<EditorRelease> release;
    QString stageRoot;
    QString assetPath;
    bool manual = false;
    bool busy = false;
    bool cancelled = false;
    bool installed = false;
    bool handedOff = false;
    QTimer downloadTimer;
};
