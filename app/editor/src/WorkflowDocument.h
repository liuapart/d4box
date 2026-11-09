#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>

class WorkflowDocument {
public:
    static WorkflowDocument createDraft();
    static WorkflowDocument createTransmuteDraft();
    static QJsonObject makeStep(const QString &action);
    static QStringList derivedRequires(const QJsonObject &data);
    void refreshRequirements();
    // Structural validation permits explicit null draft fields; forRun checks
    // static prerequisites only, never game readiness or runtime execution.
    static QStringList validate(const QJsonObject &data, bool forRun = false);
    QStringList issues(bool forRun = false) const { return validate(data, forRun); }
    static QStringList catalog(const QString &directory, QStringList *invalid = nullptr);
    bool load(const QString &path, QString *error);
    // overwrite only confirms replacing a DIFFERENT existing destination; it
    // never bypasses a loaded-file hash conflict or external deletion.
    bool saveDraft(const QString &path, QString *error, bool overwrite = false);
    QJsonObject layout() const;
    // Merge a patch into the active layout, preserving unspecified extensions.
    // Explicit calibration=manual records user confirmation, not game testing.
    void setLayout(const QJsonObject &layout);
    void setPointPosition(const QString &id, const QPointF &position);
    void setInventoryRect(const QRectF &rect);
    QString addPoint(const QString &name, const QPointF &position);
    bool removePoint(const QString &id, QString *error);
    void renamePoint(const QString &id, const QString &name);
    void setName(const QString &name);

    QJsonObject data;
    QString filePath;
    bool dirty = false;

private:
    QByteArray loadedHash;
};
