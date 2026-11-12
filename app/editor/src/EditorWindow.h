#pragma once

#include "WorkflowDocument.h"
#include <QImage>
#include <QMainWindow>
#include <functional>

class QComboBox;
class QLabel;
class QLineEdit;
class QToolBar;
class QUndoStack;
class QSystemTrayIcon;
class QAction;
class WorkflowPanel;
class OverlayLayer;
class NativeShortcuts;
class EditorUpdateController;

// A small top-level toolbar controls separate passive and interactive windows.
// It is deliberately NOT a full-screen input-receiving QWidget.
class EditorWindow : public QMainWindow {
  public:
    explicit EditorWindow(WorkflowDocument document, QString directory, QImage background,
                          int screenIndex = 0);
    ~EditorWindow() override;
    void start();
    void checkUpdates(bool manual = true);
    void showSteps();
    void closePreview();
    QImage renderPreview(bool showPanel = true);
    QJsonObject diagnostics() const;
    QJsonObject selfTest();

  protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

  private:
    bool save(bool saveAs = false);
    bool mayDiscard();
    void refresh();
    void refreshCatalog();
    void mutate(const QString &label, const std::function<void()> &edit);
    void applySnapshot(const QJsonObject &snapshot);
    void changeDocument(WorkflowDocument next, QString folder);
    void newDocument(bool useTemplate = false);
    void openDocument();
    void togglePreview();
    void toggleControls();
    void restoreControls();
    bool placeToolbar(bool avoidPoints = false);
    void selectPoint(const QString &id);
    void renamePoint(const QString &id);
    void showPointMenu(const QString &id, const QPoint &globalPosition);
    void invalidateReference();
    void showValidation();
    void updateViewIdentity(const QImage &background);
    QString viewStateKey() const;
    void loadViewState(const QString &file = {});
    bool saveViewState(const QString &file = {}) const;
    WorkflowDocument document;
    QString directory;
    QString viewIdentity;
    QRectF referenceCrop{0, 0, 1, 1};
    OverlayLayer *overlay;
    NativeShortcuts *shortcuts;
    EditorUpdateController *updates;
    WorkflowPanel *panel;
    QComboBox *profiles;
    QLineEdit *name;
    QLabel *status;
    QLabel *brand = nullptr;
    QToolBar *bar;
    QUndoStack *undo;
    QSystemTrayIcon *tray;
    QAction *operate;
    QJsonObject dragSnapshot;
    bool preview = false;
    bool controlsVisible = true;
    bool backgroundMode = false;
    bool dragging = false;
    bool panelBeforeDrag = false;
    bool controlsBeforeDrag = false;
    bool diagnosticClosing = false;
    bool exitApproved = false;
    bool movingToolbar = false;
    QPoint toolbarOffset;
    int screenIndex = 0;
};
