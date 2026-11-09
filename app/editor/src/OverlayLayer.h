#pragma once

#include <QImage>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QRectF>
#include <QSize>
#include <QString>
#include <memory>

class QWidget;
class WorkflowDocument;

// The document is borrowed, not copied. Assigning another document to *document is
// supported: call refresh() afterwards. It must outlive this object.
class OverlayLayer final : public QObject {
    Q_OBJECT
public:
    OverlayLayer(WorkflowDocument *document, int screenIndex, const QImage &background,
                 QObject *parent = nullptr);
    ~OverlayLayer() override;
    void show();
    void hide();
    void refresh();
    void setReadOnly(bool);
    void setEditingVisible(bool);
    void setDrawingVisible(bool);
    void setBackground(const QImage &);
    void setScreenIndex(int);
    void setReferenceRect(const QRectF &normalizedScreenOrImageCrop);
    QRectF referenceRect() const; // Global Qt logical coordinates, including screen offset.
    QSize referencePixelSize() const; // Image pixels, or screen logical size * screen DPR.
    QImage renderPreview(bool includeEditHandles = true) const; // Never captures the desktop.
    void selectPoint(const QString &);
    QWidget *drawingWindow() const;
    QList<QWidget *> interactiveWindows() const; // Small geometry controls only, not backdrop.
    QJsonObject diagnostics() const;

signals:
    void editStarted();
    void geometryEdited();
    void editFinished();
    void pointActivated(const QString &);
    void pointEditRequested(const QString &);
    void pointMenuRequested(const QString &, const QPoint &globalPosition);
    void restoreRequested();
    void screenChanged();

private:
    class Private;
    std::unique_ptr<Private> d;
};
