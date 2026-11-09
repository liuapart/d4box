#include "NativeOverlay.h"

#include <QAbstractNativeEventFilter>
#include <QApplication>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QPointer>
#include <QWidget>
#include <QWindow>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#ifdef Q_OS_MACOS
void d4boxMacConfigure(QWidget *, bool);
QJsonObject d4boxMacDiagnostics(QWidget *);
bool d4boxMacLeftButtonDown();
void *d4boxMacRegisterShortcuts(NativeShortcuts *, QString *);
void d4boxMacUnregisterShortcuts(void *);
#endif

namespace {
bool nativePlatform() {
    const auto platform = QGuiApplication::platformName();
#ifdef Q_OS_MACOS
    return platform == QStringLiteral("cocoa");
#elif defined(Q_OS_WIN)
    return platform == QStringLiteral("windows");
#else
    Q_UNUSED(platform);
    return false;
#endif
}
}

void nativeConfigure(QWidget *window, bool passthrough) {
    if (!window) return;
    window->setAttribute(Qt::WA_ShowWithoutActivating);
    window->setAttribute(Qt::WA_TransparentForMouseEvents, passthrough);
    if (!nativePlatform()) return;
#ifdef Q_OS_MACOS
    d4boxMacConfigure(window, passthrough);
#elif defined(Q_OS_WIN)
    const HWND handle = reinterpret_cast<HWND>(window->winId());
    LONG_PTR style = GetWindowLongPtrW(handle, GWL_EXSTYLE);
    // WS_EX_TRANSPARENT together with WS_EX_LAYERED makes the *whole* passive
    // drawing window pass through across processes, not just Qt child widgets.
    style |= WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED;
    if (passthrough) style |= WS_EX_TRANSPARENT;
    else style &= ~static_cast<LONG_PTR>(WS_EX_TRANSPARENT);
    SetWindowLongPtrW(handle, GWL_EXSTYLE, style);
    SetWindowPos(handle, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
#endif
}

QJsonObject nativeDiagnostics(QWidget *window) {
    QJsonObject result{{QStringLiteral("platform"), QGuiApplication::platformName()},
                       {QStringLiteral("nativeAvailable"), nativePlatform()}};
    if (!window) return result;
    const Qt::WindowFlags flags = window->windowFlags();
    result.insert(QStringLiteral("visible"), window->isVisible());
    result.insert(QStringLiteral("qtTransparentForInput"), bool(flags & Qt::WindowTransparentForInput));
    result.insert(QStringLiteral("qtTransparentForMouseEvents"), window->testAttribute(Qt::WA_TransparentForMouseEvents));
    result.insert(QStringLiteral("qtFrameless"), bool(flags & Qt::FramelessWindowHint));
    result.insert(QStringLiteral("qtTopmost"), bool(flags & Qt::WindowStaysOnTopHint));
    result.insert(QStringLiteral("qtDoesNotAcceptFocus"), bool(flags & Qt::WindowDoesNotAcceptFocus));
    result.insert(QStringLiteral("devicePixelRatio"), window->devicePixelRatioF());
    if (!nativePlatform() || !window->testAttribute(Qt::WA_WState_Created)) return result;
#ifdef Q_OS_MACOS
    const auto extra = d4boxMacDiagnostics(window);
    for (auto it = extra.begin(); it != extra.end(); ++it) result.insert(it.key(), it.value());
#elif defined(Q_OS_WIN)
    const HWND handle = reinterpret_cast<HWND>(window->winId());
    const auto style = GetWindowLongPtrW(handle, GWL_EXSTYLE);
    const auto regular = GetWindowLongPtrW(handle, GWL_STYLE);
    result.insert(QStringLiteral("nativeTransparent"), bool(style & WS_EX_TRANSPARENT));
    result.insert(QStringLiteral("nativeLayered"), bool(style & WS_EX_LAYERED));
    result.insert(QStringLiteral("nativeNoActivate"), bool(style & WS_EX_NOACTIVATE));
    result.insert(QStringLiteral("nativeTopmost"), bool(style & WS_EX_TOPMOST));
    result.insert(QStringLiteral("nativeBorderless"), !(regular & (WS_CAPTION | WS_THICKFRAME)));
#endif
    return result;
}

bool nativeLeftButtonDown() {
    if (nativePlatform()) {
#ifdef Q_OS_MACOS
        return d4boxMacLeftButtonDown();
#elif defined(Q_OS_WIN)
        return (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
#endif
    }
    return bool(QGuiApplication::mouseButtons() & Qt::LeftButton);
}

class NativeShortcuts::Private final : public QObject, public QAbstractNativeEventFilter {
public:
    explicit Private(NativeShortcuts *owner) : q(owner) {
        if (nativePlatform()) {
#ifdef Q_OS_MACOS
            macRegistration = d4boxMacRegisterShortcuts(q, &failure);
            registered = macRegistration != nullptr;
#elif defined(Q_OS_WIN)
            // Global atom IDs avoid collisions with another component's hotkeys.
            ids[0] = GlobalAddAtomW(L"D4BoxEditor.Overlay.Controls");
            ids[1] = GlobalAddAtomW(L"D4BoxEditor.Overlay.Preview");
            for (int i = 0; i < 2; ++i) {
                installed[i] = ids[i] && RegisterHotKey(nullptr, ids[i],
                    MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, i == 0 ? 'P' : 'D');
                if (!installed[i]) {
                    failure = QStringLiteral("RegisterHotKey failed (%1). Ctrl+Alt+P/D may be in use.").arg(GetLastError());
                    break;
                }
            }
            registered = installed[0] && installed[1];
            if (registered) qApp->installNativeEventFilter(this);
            else releaseWindows();
#endif
        }
        if (!registered) {
            if (failure.isEmpty()) failure = QStringLiteral("Global hotkeys unavailable on this platform; Ctrl+Alt+P/D are application-local only.");
            qApp->installEventFilter(this);
        }
    }
    ~Private() override {
        if (qApp) {
            qApp->removeEventFilter(this);
            qApp->removeNativeEventFilter(this);
        }
#ifdef Q_OS_MACOS
        d4boxMacUnregisterShortcuts(macRegistration);
#elif defined(Q_OS_WIN)
        releaseWindows();
#endif
    }
    bool eventFilter(QObject *, QEvent *event) override {
        if (event->type() != QEvent::KeyPress) return false;
        auto *key = static_cast<QKeyEvent *>(event);
        if (key->modifiers() != (Qt::ControlModifier | Qt::AltModifier)) return false;
        if (key->key() != Qt::Key_P && key->key() != Qt::Key_D) return false;
        if (!key->isAutoRepeat()) {
            if (key->key() == Qt::Key_P) emit q->toggleControls();
            else emit q->togglePreview();
        }
        return true;
    }
    bool nativeEventFilter(const QByteArray &, void *message, qintptr *result) override {
#ifdef Q_OS_WIN
        const auto *msg = static_cast<MSG *>(message);
        if (msg->message == WM_HOTKEY && registered) {
            if (msg->wParam == ids[0] || msg->wParam == ids[1]) {
                // Queue: a slot may destroy the owner while a native callback runs.
                QMetaObject::invokeMethod(q, msg->wParam == ids[0] ? "toggleControls" : "togglePreview", Qt::QueuedConnection);
                if (result) *result = 0;
                return true;
            }
        }
#else
        Q_UNUSED(message);
        Q_UNUSED(result);
#endif
        return false;
    }
    NativeShortcuts *q;
    bool registered = false;
    QString failure;
#ifdef Q_OS_MACOS
    void *macRegistration = nullptr;
#elif defined(Q_OS_WIN)
    ATOM ids[2] = {};
    bool installed[2] = {};
    void releaseWindows() {
        for (int i = 0; i < 2; ++i) {
            if (installed[i]) UnregisterHotKey(nullptr, ids[i]);
            if (ids[i]) GlobalDeleteAtom(ids[i]);
            installed[i] = false;
            ids[i] = 0;
        }
    }
#endif
};

NativeShortcuts::NativeShortcuts(QObject *parent) : QObject(parent), d(std::make_unique<Private>(this)) {}
NativeShortcuts::~NativeShortcuts() = default;
bool NativeShortcuts::active() const { return d->registered; }
QString NativeShortcuts::error() const { return d->failure; }
