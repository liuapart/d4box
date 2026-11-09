#include "OverlayLayer.h"
#include "NativeOverlay.h"
#include "WorkflowDocument.h"

#include <QApplication>
#include <QCursor>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QHash>
#include <QJsonArray>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QScreen>
#include <QSet>
#include <QTimer>
#include <QWidget>
#include <QWindow>
#include <algorithm>
#include <cmath>
#include <functional>

namespace {
const QColor ink(QStringLiteral("#111A23"));
const QColor surface(QStringLiteral("#20323F"));
const QColor mint(QStringLiteral("#71F4DE"));
constexpr qreal pointDiameter = 28.0;
constexpr qreal handleDiameter = 14.0;
constexpr qreal boundaryThickness = 6.0;
constexpr int rows = 3, columns = 11;

QJsonObject rectJson(const QRectF &r) {
    return {{QStringLiteral("x"), r.x()}, {QStringLiteral("y"), r.y()},
            {QStringLiteral("width"), r.width()}, {QStringLiteral("height"), r.height()}};
}
bool finiteRect(const QRectF &r) {
    return std::isfinite(r.x()) && std::isfinite(r.y()) &&
        std::isfinite(r.width()) && std::isfinite(r.height());
}
QRectF jsonRect(const QJsonObject &o) {
    for (const auto &key : {QStringLiteral("x"), QStringLiteral("y"), QStringLiteral("width"), QStringLiteral("height")})
        if (!o.value(key).isDouble()) return {};
    const QRectF rect(o.value(QStringLiteral("x")).toDouble(), o.value(QStringLiteral("y")).toDouble(),
                      o.value(QStringLiteral("width")).toDouble(), o.value(QStringLiteral("height")).toDouble());
    return finiteRect(rect) && rect.width() > 0 && rect.height() > 0 ? rect : QRectF();
}
QRectF mapped(const QRectF &n, const QRectF &reference) {
    return {reference.x() + n.x() * reference.width(), reference.y() + n.y() * reference.height(),
            n.width() * reference.width(), n.height() * reference.height()};
}
QPointF mapped(const QPointF &n, const QRectF &reference) {
    return {reference.x() + n.x() * reference.width(), reference.y() + n.y() * reference.height()};
}
QRect centered(const QPointF &p, qreal size) {
    return QRect(qRound(p.x() - size / 2), qRound(p.y() - size / 2), qRound(size), qRound(size));
}
QList<QPointF> handleCenters(const QRectF &r) {
    return {r.topLeft(), QPointF(r.center().x(), r.top()), r.topRight(),
            QPointF(r.right(), r.center().y()), r.bottomRight(),
            QPointF(r.center().x(), r.bottom()), r.bottomLeft(), QPointF(r.left(), r.center().y())};
}

class OverlayWidget final : public QWidget {
public:
    OverlayWidget(const QString &name, bool passive)
        : QWidget(nullptr, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
            Qt::WindowDoesNotAcceptFocus | (passive ? Qt::WindowTransparentForInput : Qt::WindowFlags{})) {
        setObjectName(name);
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setAttribute(Qt::WA_TransparentForMouseEvents, passive);
        setFocusPolicy(Qt::NoFocus);
        setMouseTracking(false);
    }
    std::function<void(QPainter &)> paint;
    std::function<void(const QPointF &)> press, move, release;
    std::function<void()> doubleClick;
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        if (paint) paint(painter);
    }
    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton && press) { press(event->globalPosition()); event->accept(); }
        else event->ignore();
    }
    void mouseMoveEvent(QMouseEvent *event) override {
        if (move) move(event->globalPosition());
        event->accept();
    }
    void mouseReleaseEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton && release) release(event->globalPosition());
        event->accept();
    }
    void mouseDoubleClickEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton && doubleClick) doubleClick();
        event->accept();
    }
};

void paintPoint(QPainter &p, const QPointF &center, int number, bool selected) {
    p.setPen(QPen(mint, selected ? 2.5 : 1.5));
    p.setBrush(selected ? surface : ink);
    p.drawEllipse(center, pointDiameter / 2 - 2, pointDiameter / 2 - 2);
    QFont font(QStringLiteral("sans-serif"));
    font.setPixelSize(12);
    font.setBold(true);
    p.setFont(font);
    p.drawText(QRectF(center.x() - 14, center.y() - 14, 28, 28), Qt::AlignCenter, QString::number(number));
}
}

class OverlayLayer::Private final : public QObject {
public:
    Private(OverlayLayer *owner, WorkflowDocument *doc, int index, const QImage &image)
        : QObject(owner), q(owner), document(doc), requestedScreen(index), background(image) {
        drawing = new OverlayWidget(QStringLiteral("overlayDrawing"), true);
        drawing->paint = [this](QPainter &p) {
            p.translate(-screenGeometry.topLeft());
            paintGeometry(p, editingVisible && !readOnly);
        };
        backdrop = new OverlayWidget(QStringLiteral("overlayScreenshotBackdrop"), false);
        backdrop->paint = [this](QPainter &p) {
            p.fillRect(backdrop->rect(), ink);
            p.drawImage(imageFit.translated(-screenGeometry.topLeft()), background);
        };
        // An opaque screenshot must NEVER click through to unrelated desktop
        // targets. This separate, explicitly diagnosed blocker is background-only.
        backdrop->setAccessibleName(QStringLiteral("静态参考图（不向桌面传递点击）"));
        connect(&dragPoll, &QTimer::timeout, this, [this] {
            if (!drag.active) return;
            if (nativeLeftButtonDown()) updateDrag(QCursor::pos());
            else { updateDrag(QCursor::pos()); finishDrag(); }
        });
        dragPoll.setInterval(16);
        connect(qGuiApp, &QGuiApplication::screenAdded, this, [this](QScreen *) { updateScreen(); });
        connect(qGuiApp, &QGuiApplication::screenRemoved, this, [this](QScreen *) { updateScreen(); });
        qApp->installEventFilter(this);
        updateScreen();
    }
    ~Private() override {
        qApp->removeEventFilter(this);
        dragPoll.stop();
        // No deferred deletion: top-level windows must not survive the layer.
        for (auto *control : controls) { control->hide(); delete control; }
        drawing->hide(); delete drawing;
        backdrop->hide(); delete backdrop;
    }
    struct Point { QString id, name; QPointF normalized; int number; };
    QList<Point> points() const {
        QList<Point> out;
        if (!document) return out;
        const auto positions = document->layout().value(QStringLiteral("pointPositions")).toObject();
        int number = 0;
        for (const auto &value : document->data.value(QStringLiteral("points")).toArray()) {
            ++number;
            const auto point = value.toObject();
            const auto id = point.value(QStringLiteral("id")).toString();
            const auto position = positions.value(id);
            if (id.isEmpty() || !position.isObject()) continue; // Null means not calibrated, not (0,0).
            const auto object = position.toObject();
            const auto x = object.value(QStringLiteral("x")), y = object.value(QStringLiteral("y"));
            if (!x.isDouble() || !y.isDouble() || !std::isfinite(x.toDouble()) || !std::isfinite(y.toDouble()) ||
                x.toDouble() < 0 || x.toDouble() > 1 || y.toDouble() < 0 || y.toDouble() > 1) continue;
            out.append({id, point.value(QStringLiteral("name")).toString(), {x.toDouble(), y.toDouble()}, number});
        }
        return out;
    }
    QRectF inventory() const {
        if (!document) return {};
        return jsonRect(document->layout().value(QStringLiteral("inventory")).toObject()
            .value(QStringLiteral("rect")).toObject()).intersected(QRectF(0, 0, 1, 1));
    }
    void updateScreen() {
        finishDrag();
        const auto screens = QGuiApplication::screens();
        QScreen *next = screens.isEmpty() ? nullptr : screens.at(std::clamp(requestedScreen, 0, int(screens.size()) - 1));
        if (screen != next) {
            for (const auto &connection : screenConnections) disconnect(connection);
            screenConnections.clear();
            screen = next;
            if (screen) {
                screenConnections.append(connect(screen, &QScreen::geometryChanged, this, [this] { updateScreen(); }));
                screenConnections.append(connect(screen, &QScreen::physicalDotsPerInchChanged, this, [this] { updateScreen(); }));
                screenConnections.append(connect(screen, &QScreen::logicalDotsPerInchChanged, this, [this] { updateScreen(); }));
            }
        }
        screenGeometry = screen ? screen->geometry() : QRect(0, 0, 1280, 720);
        computeReference();
        sync();
        emit q->screenChanged();
    }
    void computeReference() {
        imageFit = screenGeometry;
        if (!background.isNull()) {
            const QSizeF fitted = QSizeF(background.size()).scaled(screenGeometry.size(), Qt::KeepAspectRatio);
            imageFit = QRectF(QPointF(screenGeometry.center()) + QPointF(.5, .5) - QPointF(fitted.width()/2, fitted.height()/2), fitted);
        }
        reference = mapped(crop, imageFit);
    }
    void display(OverlayWidget *window, bool desired, bool passthrough) {
        if (!desired) { if (window->isVisible()) window->hide(); return; }
        if (!window->isVisible()) {
            window->winId();
            if (window->windowHandle() && screen) window->windowHandle()->setScreen(screen);
            nativeConfigure(window, passthrough);
            window->show();
            nativeConfigure(window, passthrough); // Cocoa may rebuild native state on show.
        }
        window->update(); // Never raise on refresh: do not steal the parent toolbar's stacking order.
    }
    void sync() {
        drawing->setGeometry(screenGeometry);
        backdrop->setGeometry(screenGeometry);
        const bool visible = shown && drawingVisible;
        display(backdrop, visible && !background.isNull(), false);
        display(drawing, visible, true);
        const bool editable = visible && editingVisible && !readOnly;
        QSet<QString> wanted;
        for (const auto &point : points()) {
            const QString key = QStringLiteral("point:") + point.id;
            wanted.insert(key);
            auto *widget = control(key, point.id, -2);
            const QPointF position = mapped(point.normalized, reference);
            widget->setGeometry(centered(position, pointDiameter));
            widget->setCursor(Qt::SizeAllCursor);
            widget->setAccessibleName(point.name);
            widget->paint = [this, widget, point](QPainter &p) {
                paintPoint(p, QPointF(widget->width()/2.0, widget->height()/2.0), point.number, point.id == selected);
            };
            display(widget, editable, false);
        }
        const auto normalized = inventory();
        if (!normalized.isEmpty()) {
            const QRectF r = mapped(normalized, reference);
            const auto centers = handleCenters(r);
            static const Qt::CursorShape cursors[] = {Qt::SizeFDiagCursor, Qt::SizeVerCursor, Qt::SizeBDiagCursor,
                Qt::SizeHorCursor, Qt::SizeFDiagCursor, Qt::SizeVerCursor, Qt::SizeBDiagCursor, Qt::SizeHorCursor};
            // Four narrow edge windows, NOT a large masked inventory window.
            // Its entire interior remains other processes' input territory.
            const QRectF edges[] = {{r.left(), r.top()-boundaryThickness/2, r.width(), boundaryThickness},
                {r.right()-boundaryThickness/2, r.top(), boundaryThickness, r.height()},
                {r.left(), r.bottom()-boundaryThickness/2, r.width(), boundaryThickness},
                {r.left()-boundaryThickness/2, r.top(), boundaryThickness, r.height()}};
            for (int i = 0; i < 4; ++i) {
                const QString key = QStringLiteral("inventory-edge:%1").arg(i);
                wanted.insert(key);
                auto *widget = control(key, {}, -1);
                widget->setGeometry(edges[i].toAlignedRect());
                widget->setCursor(Qt::SizeAllCursor);
                widget->setAccessibleName(QStringLiteral("移动格位边界"));
                widget->paint = [widget](QPainter &p) { QColor color = mint; color.setAlpha(100); p.fillRect(widget->rect(), color); };
                display(widget, editable, false);
            }
            for (int i = 0; i < 8; ++i) {
                const QString key = QStringLiteral("inventory-handle:%1").arg(i);
                wanted.insert(key);
                auto *widget = control(key, {}, i);
                widget->setGeometry(centered(centers.at(i), handleDiameter));
                widget->setCursor(cursors[i]);
                widget->setAccessibleName(QStringLiteral("调整格位边界 %1").arg(i + 1));
                widget->paint = [widget](QPainter &p) {
                    p.setPen(QPen(mint, 1.5)); p.setBrush(surface); p.drawRect(widget->rect().adjusted(2, 2, -3, -3));
                };
                display(widget, editable, false);
            }
        }
        const auto keys = controls.keys();
        for (const auto &key : keys) if (!wanted.contains(key)) {
            if (drag.active && drag.key == key) finishDrag();
            auto *widget = controls.take(key);
            widget->hide(); delete widget;
        }
    }
    OverlayWidget *control(const QString &key, const QString &point, int handle) {
        if (controls.contains(key)) return controls.value(key);
        auto *widget = new OverlayWidget(key, false);
        controls.insert(key, widget);
        widget->press = [this, key, point, handle](const QPointF &global) { beginDrag(key, point, handle, global); };
        widget->move = [this](const QPointF &global) { updateDrag(global); };
        widget->release = [this](const QPointF &global) { updateDrag(global); finishDrag(); };
        widget->doubleClick = [this, point] {
            finishDrag();
            if (!point.isEmpty() && editingVisible && !readOnly) emit q->pointEditRequested(point);
        };
        return widget;
    }
    void paintGeometry(QPainter &p, bool handles) const {
        p.save();
        p.setClipRect(screenGeometry);
        const auto normalized = inventory();
        if (!normalized.isEmpty()) {
            const QRectF r = mapped(normalized, reference);
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(mint, 1));
            p.drawRect(r);
            for (int col = 1; col < columns; ++col) {
                const qreal x = r.left() + r.width() * col / columns;
                p.drawLine(QPointF(x, r.top()), QPointF(x, r.bottom()));
            }
            for (int row = 1; row < rows; ++row) {
                const qreal y = r.top() + r.height() * row / rows;
                p.drawLine(QPointF(r.left(), y), QPointF(r.right(), y));
            }
            QFont font(QStringLiteral("sans-serif")); font.setPixelSize(11); p.setFont(font);
            for (int row = 0; row < rows; ++row) for (int col = 0; col < columns; ++col) {
                const QRectF cell(r.x() + col*r.width()/columns, r.y() + row*r.height()/rows,
                                  r.width()/columns, r.height()/rows);
                const QString number = QStringLiteral("%1,%2").arg(row+1).arg(col+1);
                p.drawText(cell.adjusted(1, 1, -1, -1), Qt::AlignCenter, number);
            }
            const QString title = QStringLiteral("格位");
            const QRectF label(r.left(), std::max(qreal(screenGeometry.top()), r.top()-23), 42, 19);
            p.fillRect(label, ink); p.drawText(label, Qt::AlignCenter, title);
            if (handles) for (const auto &center : handleCenters(r)) {
                p.setBrush(surface); p.drawRect(QRectF(center.x()-5, center.y()-5, 10, 10));
            }
        }
        for (const auto &point : points()) {
            const auto center = mapped(point.normalized, reference);
            paintPoint(p, center, point.number, handles && point.id == selected);
            QFont font(QStringLiteral("sans-serif")); font.setPixelSize(12); p.setFont(font);
            const QString name = QFontMetrics(font).elidedText(point.name, Qt::ElideRight, 128);
            const qreal width = QFontMetrics(font).horizontalAdvance(name) + 12;
            qreal x = center.x()+18;
            if (x+width > screenGeometry.right()) x = center.x()-18-width;
            const QRectF label(std::max(qreal(screenGeometry.left()), x),
                std::clamp(center.y()-11, qreal(screenGeometry.top()), qreal(screenGeometry.bottom()-21)), width, 22);
            p.setBrush(ink); p.setPen(Qt::NoPen); p.drawRoundedRect(label, 4, 4);
            p.setPen(mint); p.drawText(label, Qt::AlignCenter, name);
        }
        p.restore();
    }
    void beginDrag(const QString &key, const QString &point, int handle, const QPointF &global) {
        if (!shown || !drawingVisible || !editingVisible || readOnly || !document || reference.isEmpty()) return;
        finishDrag();
        drag.active = true; drag.key = key; drag.point = point; drag.handle = handle;
        drag.start = global; drag.reference = reference; drag.inventory = inventory();
        drag.documentId = document->data.value(QStringLiteral("id")).toString();
        drag.layoutId = document->data.value(QStringLiteral("defaultLayoutId")).toString();
        if (!point.isEmpty()) {
            bool found = false;
            for (const auto &p : points()) if (p.id == point) { drag.position = p.normalized; found = true; break; }
            if (!found) { drag = {}; return; }
            selected = point;
            emit q->pointActivated(point);
        }
        // Qt auto-grabs on press and releases on button-up. Do not re-create the
        // pressed native window or call grabMouse after moving outside its frame.
        // macOS explicitly limits grabMouse to points inside a window; the
        // active-drag-only physical-state poll also follows motion/up outside ALL
        // tiny windows (no AX permissions, event injection or desktop capture).
        const auto platform = QGuiApplication::platformName();
        if (drag.active && (platform == QStringLiteral("cocoa") || platform == QStringLiteral("windows"))) dragPoll.start();
        drawing->update();
    }
    void updateDrag(const QPointF &global) {
        if (!drag.active || !document) return;
        if (document->data.value(QStringLiteral("id")).toString() != drag.documentId ||
            document->data.value(QStringLiteral("defaultLayoutId")).toString() != drag.layoutId) { finishDrag(); return; }
        const QPointF delta = global - drag.start;
        if (!drag.started && std::hypot(delta.x(), delta.y()) < QApplication::startDragDistance()) return;
        const QPointF offset(delta.x()/drag.reference.width(), delta.y()/drag.reference.height());
        QPointF position;
        QRectF rect;
        if (!drag.point.isEmpty()) {
            position = drag.position + offset;
            position.setX(std::clamp(position.x(), 0.0, 1.0));
            position.setY(std::clamp(position.y(), 0.0, 1.0));
            const auto current = document->layout().value(QStringLiteral("pointPositions")).toObject().value(drag.point).toObject();
            if (current.isEmpty()) { finishDrag(); return; }
            if (position == QPointF(current.value(QStringLiteral("x")).toDouble(), current.value(QStringLiteral("y")).toDouble())) return;
        } else {
            rect = drag.inventory;
            if (rect.isEmpty()) return;
            if (drag.handle == -1) {
                rect.moveTopLeft({std::clamp(rect.x()+offset.x(), 0.0, 1.0-rect.width()),
                                  std::clamp(rect.y()+offset.y(), 0.0, 1.0-rect.height())});
            } else {
                const int h = drag.handle;
                const qreal minWidth = std::min(1.0, columns*16.0/drag.reference.width());
                const qreal minHeight = std::min(1.0, rows*16.0/drag.reference.height());
                if (h == 0 || h == 6 || h == 7) rect.setLeft(std::clamp(rect.left()+offset.x(), 0.0, std::max(0.0, rect.right()-minWidth)));
                if (h == 2 || h == 3 || h == 4) rect.setRight(std::clamp(rect.right()+offset.x(), std::min(1.0, rect.left()+minWidth), 1.0));
                if (h == 0 || h == 1 || h == 2) rect.setTop(std::clamp(rect.top()+offset.y(), 0.0, std::max(0.0, rect.bottom()-minHeight)));
                if (h == 4 || h == 5 || h == 6) rect.setBottom(std::clamp(rect.bottom()+offset.y(), std::min(1.0, rect.top()+minHeight), 1.0));
                // A loaded draft can already be smaller than the minimum near
                // an edge. Expand the opposite edge too when the fixed anchor
                // leaves insufficient room; never invert or leave the reference.
                rect.setSize({std::max(rect.width(), minWidth), std::max(rect.height(), minHeight)});
                rect.moveTopLeft({std::clamp(rect.left(), 0.0, 1.0-rect.width()),
                                  std::clamp(rect.top(), 0.0, 1.0-rect.height())});
            }
            if (rect == inventory()) return;
        }
        if (!drag.started) {
            drag.started = true;
            emit q->editStarted(); // History snapshot before the first setter.
            if (!drag.active || readOnly || !shown) return;
        }
        if (!drag.point.isEmpty()) document->setPointPosition(drag.point, position);
        else document->setInventoryRect(rect);
        sync();
        emit q->geometryEdited();
    }
    void finishDrag() {
        dragPoll.stop();
        const bool notify = drag.started;
        drag = {};
        if (notify) emit q->editFinished(); // Exactly once, after the last document mutation.
    }
    bool eventFilter(QObject *, QEvent *event) override {
        if (shown && event->type() == QEvent::KeyPress) {
            const auto *key = static_cast<QKeyEvent *>(event);
            if (key->key() == Qt::Key_Escape && !key->isAutoRepeat()) {
                finishDrag(); emit q->restoreRequested();
            }
        }
        return false;
    }
    OverlayLayer *q;
    WorkflowDocument *document;
    int requestedScreen;
    QPointer<QScreen> screen;
    QList<QMetaObject::Connection> screenConnections;
    QRect screenGeometry;
    QRectF imageFit, reference, crop{0, 0, 1, 1};
    QImage background;
    OverlayWidget *drawing, *backdrop;
    QHash<QString, OverlayWidget *> controls;
    QString selected;
    bool shown = false, readOnly = false, editingVisible = true, drawingVisible = true;
    struct Drag {
        bool active = false, started = false;
        QString key, point, documentId, layoutId;
        int handle = -2;
        QPointF start, position;
        QRectF inventory, reference;
    } drag;
    QTimer dragPoll;
};

OverlayLayer::OverlayLayer(WorkflowDocument *document, int screenIndex, const QImage &background, QObject *parent)
    : QObject(parent), d(std::make_unique<Private>(this, document, screenIndex, background)) {}
OverlayLayer::~OverlayLayer() { d->finishDrag(); }
void OverlayLayer::show() { d->shown = true; d->sync(); }
void OverlayLayer::hide() { d->finishDrag(); d->shown = false; d->sync(); }
void OverlayLayer::refresh() { d->sync(); }
void OverlayLayer::setReadOnly(bool value) { if (value) d->finishDrag(); d->readOnly = value; d->sync(); }
void OverlayLayer::setEditingVisible(bool value) { if (!value) d->finishDrag(); d->editingVisible = value; d->sync(); }
void OverlayLayer::setDrawingVisible(bool value) { if (!value) d->finishDrag(); d->drawingVisible = value; d->sync(); }
void OverlayLayer::setBackground(const QImage &value) {
    d->finishDrag();
    // Re-show in correct order if a backdrop is inserted beneath a live overlay.
    d->drawing->hide();
    for (auto *control : d->controls) control->hide();
    d->background = value; d->computeReference(); d->sync();
}
void OverlayLayer::setScreenIndex(int value) { d->requestedScreen = value; d->updateScreen(); }
void OverlayLayer::setReferenceRect(const QRectF &value) {
    if (!finiteRect(value) || value.width() <= 0 || value.height() <= 0) return;
    const QRectF crop = value.intersected(QRectF(0, 0, 1, 1));
    if (crop.isEmpty()) return;
    d->finishDrag(); d->crop = crop; d->computeReference(); d->sync();
}
QRectF OverlayLayer::referenceRect() const { return d->reference; }
QSize OverlayLayer::referencePixelSize() const {
    const QSizeF pixels = d->background.isNull()
        ? QSizeF(d->screenGeometry.size()) * (d->screen ? d->screen->devicePixelRatio() : 1.0)
        : QSizeF(d->background.size());
    return {std::max(1, qRound(pixels.width()*d->crop.width())), std::max(1, qRound(pixels.height()*d->crop.height()))};
}
QImage OverlayLayer::renderPreview(bool includeEditHandles) const {
    // Bounded allocation, DPR=1, no grabWindow()/screen pixels/private wallpaper.
    QSize size = d->screenGeometry.size();
    if (size.width() > 2048 || size.height() > 2048) size.scale(2048, 2048, Qt::KeepAspectRatio);
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(ink);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(qreal(size.width())/d->screenGeometry.width(), qreal(size.height())/d->screenGeometry.height());
    painter.translate(-d->screenGeometry.topLeft());
    if (!d->background.isNull()) painter.drawImage(d->imageFit, d->background);
    d->paintGeometry(painter, includeEditHandles && d->editingVisible && !d->readOnly);
    return image;
}
void OverlayLayer::selectPoint(const QString &id) { d->selected = id; d->sync(); }
QWidget *OverlayLayer::drawingWindow() const { return d->drawing; }
QList<QWidget *> OverlayLayer::interactiveWindows() const {
    QList<QWidget *> result;
    auto keys = d->controls.keys(); std::sort(keys.begin(), keys.end());
    for (const auto &key : keys) result.append(d->controls.value(key));
    return result;
}
QJsonObject OverlayLayer::diagnostics() const {
    QJsonArray windows;
    QJsonArray screens;
    for (auto *screen : QGuiApplication::screens()) {
        screens.append(QJsonObject{{QStringLiteral("name"), screen->name()},
            {QStringLiteral("geometry"), rectJson(screen->geometry())},
            {QStringLiteral("devicePixelRatio"), screen->devicePixelRatio()}});
    }
    for (auto *window : interactiveWindows()) {
        auto item = nativeDiagnostics(window);
        item.insert(QStringLiteral("name"), window->objectName());
        item.insert(QStringLiteral("geometry"), rectJson(window->geometry()));
        windows.append(item);
    }
    return {{QStringLiteral("drawing"), nativeDiagnostics(d->drawing)},
            {QStringLiteral("screens"), screens},
            {QStringLiteral("screenGeometry"), rectJson(d->screenGeometry)},
            {QStringLiteral("referenceRect"), rectJson(d->reference)},
            {QStringLiteral("normalizedCrop"), rectJson(d->crop)},
            {QStringLiteral("referencePixelWidth"), referencePixelSize().width()},
            {QStringLiteral("referencePixelHeight"), referencePixelSize().height()},
            {QStringLiteral("requestedScreenIndex"), d->requestedScreen},
            {QStringLiteral("actualScreenIndex"), QGuiApplication::screens().indexOf(d->screen)},
            {QStringLiteral("readOnly"), d->readOnly},
            {QStringLiteral("editingVisible"), d->editingVisible},
            {QStringLiteral("drawingVisible"), d->drawingVisible},
            {QStringLiteral("shown"), d->shown},
            {QStringLiteral("backgroundMode"), !d->background.isNull()},
            {QStringLiteral("backgroundBlocksDesktopInput"), d->backdrop->isVisible()},
            {QStringLiteral("backdrop"), nativeDiagnostics(d->backdrop)},
            {QStringLiteral("interactiveWindows"), windows},
            {QStringLiteral("gridRows"), rows}, {QStringLiteral("gridColumns"), columns},
            {QStringLiteral("calibratedPointCount"), d->points().size()},
            {QStringLiteral("dragActive"), d->drag.active},
            {QStringLiteral("inputInjection"), false},
            {QStringLiteral("recognition"), false}};
}
