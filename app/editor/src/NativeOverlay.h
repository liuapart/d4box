#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <memory>

class QWidget;

// Never injects input. Call on the GUI thread after window flags are established.
void nativeConfigure(QWidget *window, bool passthrough);
QJsonObject nativeDiagnostics(QWidget *window);

// Active-drag-only fallback for macOS's frame-limited explicit mouse grab.
// Reads the physical button state; neither synthesizes events nor needs AX access.
bool nativeLeftButtonDown();

// Instantiate ONE in the parent controller. OverlayLayer does not register a
// second copy. active() means both GLOBAL shortcuts registered successfully;
// Linux/offscreen provide an app-local fallback and active() remains false.
class NativeShortcuts final : public QObject {
    Q_OBJECT
public:
    explicit NativeShortcuts(QObject *parent = nullptr);
    ~NativeShortcuts() override;
    bool active() const;
    QString error() const;
signals:
    void toggleControls(); // Ctrl+Alt+P (Control+Option+P on macOS)
    void togglePreview();  // Ctrl+Alt+D
private:
    class Private;
    std::unique_ptr<Private> d;
};
