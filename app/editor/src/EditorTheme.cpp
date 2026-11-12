#include "EditorTheme.h"
#include <QApplication>
#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QLineEdit>
#include <QImageReader>
#include <QSvgRenderer>
#include <QAbstractScrollArea>
#include <QEvent>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QScrollBar>
#include <QTimer>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QProxyStyle>
#include <QStyleFactory>
#include <QStyleOption>
#include <QWheelEvent>

namespace {
// Value controls never consume scrolling as an edit, even when focused.
// Forward to the surrounding form so the gesture does not stall over a field.
class ValueWheelGuard final : public QObject {
public:
    explicit ValueWheelGuard(QWidget *box) : QObject(box), box(box) {
        box->installEventFilter(this);
        // Spin boxes contain a line editor; wheel delivery can target it too.
        for (auto *editor : box->findChildren<QLineEdit *>(QString(), Qt::FindDirectChildrenOnly))
            editor->installEventFilter(this);
    }
protected:
    bool eventFilter(QObject *, QEvent *event) override {
        if (event->type() != QEvent::Wheel) return false;
        if (auto *combo = qobject_cast<QComboBox *>(box); combo && combo->view()->isVisible())
            return false; // The open list retains normal scrolling.
        auto *wheel = static_cast<QWheelEvent *>(event);
        for (auto *parent = box->parentWidget(); parent; parent = parent->parentWidget()) {
            if (auto *area = qobject_cast<QAbstractScrollArea *>(parent)) {
                auto *viewport = area->viewport();
                QWheelEvent forwarded(viewport->mapFromGlobal(wheel->globalPosition()),
                                      wheel->globalPosition(), wheel->pixelDelta(), wheel->angleDelta(),
                                      wheel->buttons(), wheel->modifiers(), wheel->phase(),
                                      wheel->inverted(), wheel->source(), wheel->pointingDevice());
                QApplication::sendEvent(viewport, &forwarded);
                // The viewport has handled propagation; do not scroll twice.
                event->accept();
                return true;
            }
        }
        event->ignore();
        return true;
    }
private:
    QWidget *box;
};

// Keep the scrollbar's hit area and layout gutter intact while hiding its paint.
// Unlike changing scroll policies, this never reflows the form or steals wheel input.
class AutoHideScrollBar final : public QObject {
public:
    explicit AutoHideScrollBar(QScrollBar *bar) : QObject(bar), bar(bar) {
        effect = new QGraphicsOpacityEffect(bar);
        effect->setOpacity(0);
        bar->setGraphicsEffect(effect);
        fade = new QPropertyAnimation(effect, "opacity", this);
        fade->setDuration(150);
        idle.setObjectName(QStringLiteral("scrollbarIdleTimer"));
        idle.setParent(this);
        idle.setSingleShot(true);
        idle.setInterval(1200);
        bar->installEventFilter(this);
        for (QWidget *parent = bar->parentWidget(); parent; parent = parent->parentWidget()) {
            if (auto *area = qobject_cast<QAbstractScrollArea *>(parent)) {
                area->viewport()->installEventFilter(this);
                break;
            }
        }
        connect(bar, &QScrollBar::valueChanged, this, [this] { reveal(); });
        connect(bar, &QScrollBar::sliderPressed, this, [this] { reveal(); });
        connect(bar, &QScrollBar::sliderReleased, this, [this] { idle.start(); });
        connect(&idle, &QTimer::timeout, this, [this] {
            if (hovered || this->bar->isSliderDown()) return;
            fade->stop();
            fade->setStartValue(effect->opacity());
            fade->setEndValue(0.0);
            fade->start();
        });
    }
protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (watched == bar) {
            if (event->type() == QEvent::Enter) hovered = true;
            if (event->type() == QEvent::Leave) { hovered = false; idle.start(); }
            if (event->type() == QEvent::Hide) {
                hovered = false; idle.stop(); fade->stop(); effect->setOpacity(0);
            }
        }
        switch (event->type()) {
        case QEvent::Enter:
        case QEvent::Wheel:
        case QEvent::KeyPress:
        case QEvent::MouseButtonPress:
            reveal();
            break;
        default: break;
        }
        return false;
    }
private:
    void reveal() {
        if (!bar->isVisible() || bar->maximum() <= bar->minimum()) return;
        fade->stop(); effect->setOpacity(1); idle.start();
    }
    QScrollBar *bar;
    QGraphicsOpacityEffect *effect;
    QPropertyAnimation *fade;
    QTimer idle;
    bool hovered = false;
};

class ControlsStyle final : public QProxyStyle {
public:
    ControlsStyle() : QProxyStyle(QStyleFactory::create(QStringLiteral("Fusion"))) {}
    void polish(QWidget *widget) override {
        QProxyStyle::polish(widget);
        if (qobject_cast<QComboBox *>(widget) || qobject_cast<QAbstractSpinBox *>(widget)) {
            if (!widget->property("d4boxValueWheelGuard").toBool()) {
                widget->setProperty("d4boxValueWheelGuard", true);
                new ValueWheelGuard(widget);
            }
        }
        if (auto *bar = qobject_cast<QScrollBar *>(widget)) {
            if (!bar->property("d4boxAutoHide").toBool()) {
                bar->setProperty("d4boxAutoHide", true);
                new AutoHideScrollBar(bar);
            }
        }
    }
    int styleHint(StyleHint hint, const QStyleOption *option = nullptr,
                  const QWidget *widget = nullptr, QStyleHintReturn *data = nullptr) const override {
        if (hint == SH_ComboBox_Popup || hint == SH_ScrollBar_Transient) return 0;
        // Scrolling a form must not change even a focused combo's value.
        // Also keep Qt's own wheel-to-selection behavior disabled; popup item
        // views retain normal scrolling independently of this style hint.
        if (hint == SH_ComboBox_AllowWheelScrolling) return 0;
        return QProxyStyle::styleHint(hint, option, widget, data);
    }
    void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *p,
                       const QWidget *widget = nullptr) const override {
        if (element != PE_IndicatorArrowDown && element != PE_IndicatorArrowUp &&
            element != PE_IndicatorArrowLeft && element != PE_IndicatorArrowRight &&
            element != PE_IndicatorSpinUp && element != PE_IndicatorSpinDown) {
            QProxyStyle::drawPrimitive(element, option, p, widget);
            return;
        }
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        p->translate(QRectF(option->rect).center());
        if (element == PE_IndicatorArrowUp || element == PE_IndicatorSpinUp) p->rotate(180);
        if (element == PE_IndicatorArrowLeft) p->rotate(90);
        if (element == PE_IndicatorArrowRight) p->rotate(-90);
        p->setPen(QPen(option->palette.color(option->state & State_Enabled ? QPalette::Active : QPalette::Disabled,
                                             QPalette::ButtonText), 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        QPainterPath path; path.moveTo(-3.5, -1.5); path.lineTo(0, 2); path.lineTo(3.5, -1.5);
        p->drawPath(path);
        p->restore();
    }
};
}

void EditorTheme::install(QApplication &app) {
    app.setStyle(new ControlsStyle);
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(QStringLiteral("#111A23")));
    palette.setColor(QPalette::WindowText, QColor(QStringLiteral("#E8F0F5")));
    palette.setColor(QPalette::Base, QColor(QStringLiteral("#15222D")));
    palette.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#1B2B37")));
    palette.setColor(QPalette::Text, QColor(QStringLiteral("#E8F0F5")));
    palette.setColor(QPalette::Button, QColor(QStringLiteral("#20323F")));
    palette.setColor(QPalette::ButtonText, QColor(QStringLiteral("#CFDFE8")));
    palette.setColor(QPalette::Highlight, QColor(QStringLiteral("#295047")));
    palette.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#ECFFF9")));
    palette.setColor(QPalette::PlaceholderText, QColor(QStringLiteral("#91A5B4")));
    for (auto role : {QPalette::Text, QPalette::ButtonText, QPalette::WindowText})
        palette.setColor(QPalette::Disabled, role, QColor(QStringLiteral("#748794")));
    app.setPalette(palette);
    app.setStyleSheet(QString::fromLatin1(R"QSS(
QWidget { color:#E8F0F5; font-size:13px; }
QMainWindow#editorToolbar { background:transparent; }
QToolBar#commandBar { background:#20323F; border:1px solid #435764; border-radius:12px; padding:8px 10px; spacing:4px; }
QToolBar::separator { background:#3A4D59; width:1px; margin:8px 7px; }
QLabel#brand { color:#71F4DE; font-size:14px; font-weight:700; padding:0 8px 0 2px; }
QLabel#saveState { color:#C1D2DC; font-size:11px; padding:3px 5px; background:#2B3E4B; border-radius:5px; }
QWidget#stepsPanel, QDialog[d4boxFrameless=true] { background:transparent; }
QFrame#panelSurface { background:#111A23; border:1px solid #435764; border-radius:12px; }
QWidget#panelTitleBar { background:transparent; }
QToolButton#panelClose { background:transparent; border:1px solid transparent; padding:0; }
QToolButton#panelClose:hover { background:#304A55; border-color:#486775; }
QToolButton#panelClose:focus { border-color:#71F4DE; }
QDialog, QTabWidget::pane { background:#111A23; }
QTabWidget::pane { border:0; border-top:1px solid #30424F; }
QScrollArea, QScrollArea > QWidget > QWidget { background:#111A23; border:0; }
QLabel[role=heading] { color:#E8F0F5; font-size:14px; font-weight:600; padding:2px 0 4px; }
QLabel[role=hint] { color:#9DB0BE; font-size:12px; padding:3px 0; }
QPushButton, QToolButton { background:#20323F; border:1px solid #3D5260; border-radius:6px; padding:6px 10px; min-height:18px; }
QPushButton:hover, QToolButton:hover { background:#2B434D; border-color:#65938F; }
QPushButton:pressed, QToolButton:pressed, QToolButton:checked { background:#295047; border-color:#71F4DE; }
QPushButton:focus, QToolButton:focus { border-color:#71F4DE; }
QToolBar QToolButton { background:transparent; border:1px solid transparent; padding:6px 8px; }
QToolBar QToolButton:hover { background:#304A55; border-color:#486775; }
QToolBar QToolButton:pressed, QToolBar QToolButton:checked { background:#295047; border-color:#71F4DE; }
QToolBar QToolButton:focus { border-color:#71F4DE; }
QToolButton#moreButton { padding-right:23px; }
QToolButton#moreButton::menu-indicator { image:url(:/theme/down.svg); subcontrol-origin:padding; subcontrol-position:right center; right:7px; width:10px; height:10px; }
QToolButton[role=primary], QPushButton[role=primary] { background:#71F4DE; color:#102A2A; border:1px solid #71F4DE; font-weight:600; }
QToolButton[role=primary]:hover, QPushButton[role=primary]:hover { background:#9BF9E9; border-color:#9BF9E9; }
QToolButton[role=primary]:pressed, QPushButton[role=primary]:pressed { background:#50C9B7; border-color:#50C9B7; }
QPushButton[danger=true] { color:#E8AFB5; border-color:#65444F; background:#2A242F; }
QPushButton[danger=true]:hover { background:#44303B; border-color:#BE7D88; }
QPushButton:disabled, QToolButton:disabled { color:#748794; background:#192630; border-color:#2B3D49; }
QPushButton::menu-indicator { subcontrol-origin:padding; subcontrol-position:right center; right:9px; width:9px; height:9px; }
QPushButton[menuButton=true] { padding-right:24px; }
QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox { background:#15222D; border:1px solid #3C5261; border-radius:6px; padding:6px 9px; min-height:18px; selection-background-color:#295047; selection-color:#ECFFF9; }
QLineEdit:hover, QComboBox:hover, QSpinBox:hover, QDoubleSpinBox:hover { border-color:#63808E; }
QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus { border-color:#71F4DE; background:#192D36; }
QLineEdit:disabled, QComboBox:disabled, QSpinBox:disabled, QDoubleSpinBox:disabled { color:#748794; background:#142029; border-color:#293B47; }
QLineEdit#profileName { background:transparent; border-color:transparent; }
QLineEdit#profileName:hover { background:#1B2B37; border-color:#435B68; }
QLineEdit#profileName:focus { background:#15222D; border-color:#71F4DE; }
QComboBox { padding-right:32px; }
QComboBox::drop-down { subcontrol-origin:padding; subcontrol-position:top right; width:28px; border:0; border-left:1px solid #304552; border-top-right-radius:6px; border-bottom-right-radius:6px; background:#203441; }
QComboBox::drop-down:hover { background:#304D56; }
QComboBox::down-arrow { image:url(:/theme/down.svg); width:12px; height:12px; }
QComboBox QAbstractItemView { background:#1B2B37; border:1px solid #52717D; padding:5px; outline:0; selection-background-color:#295047; selection-color:#ECFFF9; }
QComboBox QAbstractItemView::item { min-height:30px; padding:2px 10px; border-radius:4px; }
QComboBox QAbstractItemView::item:hover { background:#2A424D; }
QComboBox QAbstractItemView::item:selected { background:#295047; color:#91F5E0; }
/* Reserve a separate button strip outside the input frame so focus never bleeds around it. */
QSpinBox, QDoubleSpinBox { margin-right:26px; border-top-right-radius:0; border-bottom-right-radius:0; }
QSpinBox::up-button, QDoubleSpinBox::up-button { subcontrol-origin:margin; subcontrol-position:top right; width:25px; border:1px solid #3C5261; border-left:0; border-bottom-color:#304552; border-top-right-radius:6px; background:#203441; }
QSpinBox::down-button, QDoubleSpinBox::down-button { subcontrol-origin:margin; subcontrol-position:bottom right; width:25px; border:1px solid #3C5261; border-left:0; border-top:0; border-bottom-right-radius:6px; background:#203441; }
QSpinBox::up-button:hover, QDoubleSpinBox::up-button:hover, QSpinBox::down-button:hover, QDoubleSpinBox::down-button:hover { background:#304D56; }
QSpinBox::up-arrow, QDoubleSpinBox::up-arrow, QSpinBox::down-arrow, QDoubleSpinBox::down-arrow { width:10px; height:8px; }
QSpinBox::up-arrow, QDoubleSpinBox::up-arrow { image:url(:/theme/up.svg); }
QSpinBox::down-arrow, QDoubleSpinBox::down-arrow { image:url(:/theme/down.svg); }
QScrollBar:vertical { background:#111A23; width:12px; margin:3px 2px; border:0; }
QScrollBar::handle:vertical { background:#526B79; border-radius:4px; min-height:36px; }
QScrollBar::handle:vertical:hover { background:#7DA89F; }
QScrollBar::handle:vertical:pressed { background:#71F4DE; }
QScrollBar:horizontal { background:#111A23; height:12px; margin:2px 3px; border:0; }
QScrollBar::handle:horizontal { background:#526B79; border-radius:4px; min-width:36px; }
QScrollBar::handle:horizontal:hover { background:#7DA89F; }
QScrollBar::handle:horizontal:pressed { background:#71F4DE; }
QScrollBar::add-line, QScrollBar::sub-line { width:0; height:0; border:0; background:none; }
QScrollBar::add-page, QScrollBar::sub-page { background:none; }
QTreeView { background:#15222D; alternate-background-color:#1B2B37; border:1px solid #304754; selection-background-color:#295047; selection-color:#AEF9E9; }
QTreeView::item { min-height:28px; }
QHeaderView::section { background:#20323F; color:#CFDFE8; border:1px solid #304754; padding:5px; }
QPlainTextEdit { background:#15222D; border:1px solid #304754; }
QProgressBar { background:#15222D; border:1px solid #3C5261; border-radius:5px; min-height:10px; }
QProgressBar::chunk { background:#71F4DE; border-radius:4px; }
QLabel#fileError { color:#F1C27A; }
QListWidget { background:#15222D; border:1px solid #304754; border-radius:7px; outline:0; }
QListWidget::item { padding:10px; border-bottom:1px solid #243744; }
QListWidget::item:hover { background:#203641; }
QListWidget::item:selected { background:#26473F; color:#AEF9E9; }
QListWidget:focus { border-color:#71F4DE; }
QTabBar::tab { background:transparent; color:#A4B7C4; padding:12px 24px; border-bottom:2px solid transparent; }
QTabBar::tab:hover { background:#1D303C; color:#E8F0F5; }
QTabBar::tab:selected { background:#1E363A; color:#8AF5DF; border-bottom:2px solid #71F4DE; }
QCheckBox { spacing:6px; }
QCheckBox::indicator { width:16px; height:16px; }
QCheckBox::indicator:unchecked { background:#15222D; border:1px solid #617B89; border-radius:4px; }
QCheckBox::indicator:unchecked:hover { border-color:#71F4DE; background:#203C40; }
QCheckBox::indicator:checked { background:#71F4DE; border:1px solid #71F4DE; border-radius:4px; image:url(:/theme/check.svg); }
QMenu { background:#1B2B37; border:1px solid #52717D; padding:5px; }
QMenu::item { padding:8px 24px; border-radius:4px; }
QMenu::item:selected { background:#295047; color:#A3F8E7; }
QMenu::item:disabled { color:#748794; }
QMenu::separator { height:1px; background:#364B58; margin:5px 10px; }
QLabel#validation { color:#AFC2CD; font-size:12px; background:#192A35; border:1px solid #2C414E; border-radius:6px; padding:7px; }
QToolTip { color:#E8F0F5; background:#20323F; border:1px solid #52717D; padding:6px; }
)QSS"));
}

bool EditorTheme::validateResources(QString *error) {
    // Exercise both SVG plugins as well as the vector renderer in packaged smoke
    // tests: SDK-installed plugins must not mask missing deployment dependencies.
    for (const auto *name : {":/theme/down.svg", ":/theme/up.svg", ":/theme/check.svg"}) {
        const auto path = QString::fromLatin1(name);
        QSvgRenderer renderer(path);
        if (!renderer.isValid()) {
            if (error) *error = QStringLiteral("Invalid embedded SVG: ") + path;
            return false;
        }
        for (int size : {16, 24, 32, 48, 64}) {
            QImageReader reader(path);
            reader.setScaledSize(QSize(size, size));
            const auto image = reader.read();
            bool painted = false;
            for (int y = 0; y < image.height() && !painted; ++y)
                for (int x = 0; x < image.width() && !painted; ++x)
                    painted = image.pixelColor(x, y).alpha() > 0;
            if (image.size() != QSize(size, size) || !painted) {
                if (error) *error = QStringLiteral("SVG image plugin failed: ") + path + reader.errorString();
                return false;
            }
        }
        const auto pixmap = QIcon(path).pixmap(QSize(64, 64));
        if (pixmap.isNull() || pixmap.width() < 64 || pixmap.height() < 64) {
            if (error) *error = QStringLiteral("SVG icon plugin failed: ") + path;
            return false;
        }
    }
    return true;
}

QIcon EditorTheme::icon(Symbol symbol, bool primary) {
    QIcon result;
    for (int scale : {1, 2, 3}) {
        QPixmap pixmap(16 * scale, 16 * scale); pixmap.setDevicePixelRatio(scale); pixmap.fill(Qt::transparent);
        QPainter p(&pixmap); p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(QColor(primary ? QStringLiteral("#102A2A") : QStringLiteral("#BDCFDA")), 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        QPainterPath path;
        switch (symbol) {
        case Symbol::Add: p.drawLine(QPointF(8,3), QPointF(8,13)); p.drawLine(QPointF(3,8), QPointF(13,8)); break;
        case Symbol::Grid: p.drawRoundedRect(QRectF(2,2,12,12),2,2); p.drawLine(QPointF(6,2),QPointF(6,14)); p.drawLine(QPointF(10,2),QPointF(10,14)); p.drawLine(QPointF(2,8),QPointF(14,8)); break;
        case Symbol::Steps: for (int y : {4,8,12}) { p.drawPoint(QPointF(2,y)); p.drawLine(QPointF(6,y),QPointF(14,y)); } break;
        case Symbol::Eye: path.moveTo(1,8); path.quadTo(8,-1,15,8); path.quadTo(8,17,1,8); p.drawPath(path); p.drawEllipse(QPointF(8,8),2,2); break;
        case Symbol::Pointer: path.moveTo(3,2); path.lineTo(13,8); path.lineTo(8,9); path.lineTo(6,14); path.closeSubpath(); p.drawPath(path); break;
        case Symbol::Save: p.drawRoundedRect(QRectF(2,2,12,12),1.5,1.5); p.drawRect(QRectF(5,2,6,4)); p.drawRect(QRectF(5,9,6,5)); break;
        case Symbol::Close: p.drawLine(QPointF(4,4),QPointF(12,12)); p.drawLine(QPointF(4,12),QPointF(12,4)); break;
        }
        p.end(); result.addPixmap(pixmap);
    }
    return result;
}
