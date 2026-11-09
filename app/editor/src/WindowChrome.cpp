#include "WindowChrome.h"
#include "EditorTheme.h"
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QScreen>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

WindowChrome::WindowChrome(QWidget *host, const QString &caption)
    : QObject(host), host(host) {
    host->setWindowFlag(Qt::FramelessWindowHint);
    host->setWindowFlag(Qt::WindowStaysOnTopHint);
    host->setAttribute(Qt::WA_TranslucentBackground);
    host->installEventFilter(this);
    auto *outer = new QVBoxLayout(host);
    outer->setContentsMargins(0, 0, 0, 0);
    auto *surface = new QFrame(host);
    surface->setObjectName(QStringLiteral("panelSurface"));
    outer->addWidget(surface);
    auto *root = new QVBoxLayout(surface);
    root->setContentsMargins(10, 8, 10, 8);
    root->setSpacing(10);
    titleBar = new QWidget(surface);
    titleBar->setObjectName(QStringLiteral("panelTitleBar"));
    titleBar->setCursor(Qt::SizeAllCursor);
    titleBar->setToolTip(QStringLiteral("按住标题区域拖动面板"));
    titleBar->installEventFilter(this);
    auto *header = new QHBoxLayout(titleBar);
    header->setContentsMargins(6, 0, 0, 0);
    auto *title = new QLabel(caption, titleBar);
    title->setProperty("role", QStringLiteral("heading"));
    title->setAttribute(Qt::WA_TransparentForMouseEvents);
    header->addWidget(title);
    auto *hint = new QLabel(QStringLiteral("拖动此处移动"), titleBar);
    hint->setProperty("role", QStringLiteral("hint"));
    hint->setAttribute(Qt::WA_TransparentForMouseEvents);
    header->addWidget(hint);
    header->addStretch();
    auto *close = new QToolButton(titleBar);
    close->setObjectName(QStringLiteral("panelClose"));
    close->setIcon(EditorTheme::icon(EditorTheme::Symbol::Close));
    close->setAccessibleName(QStringLiteral("关闭"));
    close->setToolTip(QStringLiteral("关闭"));
    close->setCursor(Qt::PointingHandCursor);
    close->setFixedSize(32, 32);
    header->addWidget(close);
    connect(close, &QToolButton::clicked, host, &QWidget::close);
    root->addWidget(titleBar);
    content = root;
}

bool WindowChrome::eventFilter(QObject *watched, QEvent *event) {
    if (watched == host && (event->type() == QEvent::Hide || event->type() == QEvent::WindowDeactivate))
        movingPanel = false;
    if (watched != titleBar)
        return QObject::eventFilter(watched, event);
    if (event->type() == QEvent::MouseButtonPress) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() == Qt::LeftButton) {
            movingPanel = true;
            dragOffset = mouse->globalPosition().toPoint() - host->pos();
            return true;
        }
    } else if (event->type() == QEvent::MouseMove && movingPanel) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (!(mouse->buttons() & Qt::LeftButton)) {
            movingPanel = false;
            return false;
        }
        const auto global = mouse->globalPosition().toPoint();
        auto next = global - dragOffset;
        auto *targetScreen = QGuiApplication::screenAt(global);
        if (!targetScreen) targetScreen = host->screen();
        if (targetScreen) {
            const auto available = targetScreen->availableGeometry();
            // Keep the title and close control reachable, including on smaller displays.
            next.setX(qBound(available.left(), next.x(), qMax(available.left(), available.right() - host->width() + 1)));
            next.setY(qBound(available.top(), next.y(), qMax(available.top(), available.bottom() - titleBar->height() + 1)));
        }
        host->move(next);
        return true;
    } else if (event->type() == QEvent::MouseButtonRelease || event->type() == QEvent::Hide ||
               event->type() == QEvent::UngrabMouse || event->type() == QEvent::WindowDeactivate) {
        movingPanel = false;
    }
    return QObject::eventFilter(watched, event);
}
