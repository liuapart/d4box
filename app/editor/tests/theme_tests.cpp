#include "EditorTheme.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QAbstractItemView>
#include <QDir>
#include <QDoubleSpinBox>
#include <QEnterEvent>
#include <QEventLoop>
#include <QGraphicsOpacityEffect>
#include <QTimer>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QStyleOptionSpinBox>
#include <QStyledItemDelegate>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <iostream>

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    EditorTheme::install(app);
    int failures = 0;
    auto check = [&](bool ok, const char *message) {
        if (!ok) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
    };
    auto wait = [](int ms) {
        QEventLoop loop;
        QTimer::singleShot(ms, &loop, &QEventLoop::quit);
        loop.exec();
    };
    for (const auto *name : {":/theme/down.svg", ":/theme/up.svg", ":/theme/check.svg"})
        check(!QImage(QString::fromLatin1(name)).isNull(), "embedded glyph decodes");
    QString resourceError;
    const bool resourcesValid = EditorTheme::validateResources(&resourceError);
    check(resourcesValid, qPrintable(resourceError));
    QWidget window;
    window.resize(460, 510);
    auto *layout = new QVBoxLayout(&window);
    auto *heading = new QLabel(QStringLiteral("编辑器控件 · 深色主题"));
    heading->setProperty("role", QStringLiteral("heading"));
    layout->addWidget(heading);
    auto *combo = new QComboBox;
    combo->setItemDelegate(new QStyledItemDelegate(combo));
    combo->setMaxVisibleItems(6);
    for (int i = 0; i < 18; ++i)
        combo->addItem(QStringLiteral("步骤 %1 · 点击目标点位").arg(i + 1));
    layout->addWidget(combo);
    auto *spin = new QSpinBox;
    spin->setRange(0, 10000); spin->setValue(250); spin->setSuffix(QStringLiteral(" ms"));
    layout->addWidget(spin);
    auto *fraction = new QDoubleSpinBox;
    fraction->setRange(0, 1); fraction->setDecimals(5); fraction->setSingleStep(.001);
    fraction->setValue(.5625); layout->addWidget(fraction);
    layout->addWidget(new QLineEdit(QStringLiteral("自动嬗变背包物品")));
    auto *disabled = new QComboBox;
    disabled->addItem(QStringLiteral("禁用状态")); disabled->setEnabled(false);
    layout->addWidget(disabled);
    auto *checkbox = new QCheckBox(QStringLiteral("遍历背包中的物品"));
    checkbox->setChecked(true); layout->addWidget(checkbox);
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    auto *content = new QWidget;
    auto *rows = new QVBoxLayout(content);
    auto *formCombo = new QComboBox;
    formCombo->addItems({QStringLiteral("目标一"), QStringLiteral("目标二"), QStringLiteral("目标三")});
    formCombo->setCurrentIndex(1);
    rows->addWidget(formCombo);
    auto *formSpin = new QSpinBox;
    formSpin->setRange(0, 10000); formSpin->setValue(250);
    rows->addWidget(formSpin);
    auto *formFraction = new QDoubleSpinBox;
    formFraction->setRange(0, 1); formFraction->setDecimals(5); formFraction->setSingleStep(.001);
    formFraction->setValue(.5625); rows->addWidget(formFraction);
    for (int i = 0; i < 20; ++i) {
        auto *label = new QLabel(QStringLiteral("动作 %1 · 等待界面就绪").arg(i + 1));
        label->setMinimumHeight(30); rows->addWidget(label);
    }
    scroll->setWidget(content); layout->addWidget(scroll);
    auto *save = new QPushButton(QStringLiteral("保存方案"));
    save->setProperty("role", QStringLiteral("primary")); layout->addWidget(save);
    window.show(); app.processEvents();
    // Unlike activateWindow(), this test-only call sets Qt's active widget
    // without asking the OS to steal focus from the terminal on macOS.
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    QApplication::setActiveWindow(&window);
    QT_WARNING_POP
    check(scroll->verticalScrollBar()->maximum() > 0, "long content scrolls");
    scroll->verticalScrollBar()->setValue(80);
    check(scroll->verticalScrollBar()->value() == 80, "scrollbar can move");
    combo->setFocus();
    QKeyEvent down(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier);
    QApplication::sendEvent(combo, &down);
    check(combo->currentIndex() == 1, "combo keyboard navigation");
    auto wheel = [](QWidget *widget, const QPoint &angle, const QPoint &pixels = QPoint{},
                    Qt::ScrollPhase phase = Qt::NoScrollPhase, QPoint local = QPoint(-1, -1)) {
        if (local == QPoint(-1, -1)) local = widget->rect().center();
        QWheelEvent event(local, widget->mapToGlobal(local), pixels, angle,
                          Qt::NoButton, Qt::NoModifier, phase, false);
        QApplication::sendEvent(widget, &event);
    };
    int comboActivations = 0, comboChanges = 0, formActivations = 0, formChanges = 0;
    QObject::connect(combo, &QComboBox::activated, &window, [&](int) { ++comboActivations; });
    QObject::connect(combo, &QComboBox::currentIndexChanged, &window, [&](int) { ++comboChanges; });
    QObject::connect(formCombo, &QComboBox::activated, &window, [&](int) { ++formActivations; });
    QObject::connect(formCombo, &QComboBox::currentIndexChanged, &window, [&](int) { ++formChanges; });
    for (bool focused : {false, true}) {
        if (focused) combo->setFocus(); else spin->setFocus();
        for (int direction : {-120, 120}) wheel(combo, QPoint(0, direction));
        check(combo->currentIndex() == 1, "closed combo ignores wheel even with focus");
        if (focused) formCombo->setFocus(); else spin->setFocus();
        scroll->verticalScrollBar()->setValue(0);
        wheel(formCombo, QPoint(0, -120));
        check(formCombo->currentIndex() == 1, "wheel over form combo keeps selection");
        check(scroll->verticalScrollBar()->value() > 0, "wheel over combo scrolls containing page");
    }
    scroll->verticalScrollBar()->setValue(0);
    wheel(formCombo, {}, {}, Qt::ScrollBegin);
    wheel(formCombo, QPoint(0, -120), QPoint(0, -40), Qt::ScrollUpdate);
    wheel(formCombo, {}, {}, Qt::ScrollEnd);
    check(formCombo->currentIndex() == 1 && scroll->verticalScrollBar()->value() > 0,
          "touchpad sequence scrolls form without changing target");
    check(comboActivations == 0 && comboChanges == 0 && formActivations == 0 && formChanges == 0,
          "closed combo wheel emits no selection or activation signals");
    auto spinRect = [](QAbstractSpinBox *box, QStyle::SubControl subControl) {
        QStyleOptionSpinBox option;
        option.initFrom(box);
        option.frame = box->hasFrame();
        option.buttonSymbols = box->buttonSymbols();
        option.stepEnabled = QAbstractSpinBox::StepUpEnabled | QAbstractSpinBox::StepDownEnabled;
        return box->style()->subControlRect(QStyle::CC_SpinBox, &option, subControl, box);
    };
    int spinChanges = 0;
    for (auto *box : {spin, formSpin})
        QObject::connect(box, &QSpinBox::valueChanged, &window, [&](int) { ++spinChanges; });
    for (auto *box : {fraction, formFraction})
        QObject::connect(box, &QDoubleSpinBox::valueChanged, &window, [&](double) { ++spinChanges; });
    const QList<QAbstractSpinBox *> numericFields{spin, fraction, formSpin, formFraction};
    for (auto *box : numericFields) {
        auto *editor = box->findChild<QLineEdit *>();
        check(editor != nullptr, "numeric control has text editor");
        for (bool focused : {false, true}) {
            if (focused) box->setFocus(); else combo->setFocus();
            app.processEvents();
            check(box->hasFocus() == focused, "numeric wheel tested with and without actual focus");
            const bool inForm = box == formSpin || box == formFraction;
            for (const auto position : {box->rect().center(),
                                       spinRect(box, QStyle::SC_SpinBoxUp).center(),
                                       spinRect(box, QStyle::SC_SpinBoxDown).center()}) {
                scroll->verticalScrollBar()->setValue(0);
                wheel(box, QPoint(0, -120), {}, Qt::NoScrollPhase, position);
                if (inForm)
                    check(scroll->verticalScrollBar()->value() > 0,
                          "wheel over numeric input and buttons scrolls containing page");
                wheel(box, QPoint(0, 120), {}, Qt::NoScrollPhase, position);
                if (inForm)
                    check(scroll->verticalScrollBar()->value() == 0, "numeric wheel also scrolls page up");
            }
            if (editor) {
                scroll->verticalScrollBar()->setValue(0);
                wheel(editor, QPoint(0, -120));
                if (inForm)
                    check(scroll->verticalScrollBar()->value() > 0, "wheel on child editor scrolls page");
                scroll->verticalScrollBar()->setValue(0);
                wheel(editor, {}, {}, Qt::ScrollBegin);
                wheel(editor, QPoint(0, -120), QPoint(0, -40), Qt::ScrollUpdate);
                wheel(editor, {}, {}, Qt::ScrollEnd);
                if (inForm)
                    check(scroll->verticalScrollBar()->value() > 0, "trackpad over numeric editor scrolls page");
            }
            check(spin->value() == 250 && formSpin->value() == 250 &&
                  fraction->value() == .5625 && formFraction->value() == .5625,
                  "wheel never edits integer or decimal values, including focused fields");
        }
    }
    check(spinChanges == 0, "numeric wheel emits no valueChanged signals");
    auto click = [](QWidget *widget, const QPoint &pos) {
        QMouseEvent press(QEvent::MouseButtonPress, pos, widget->mapToGlobal(pos),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QMouseEvent release(QEvent::MouseButtonRelease, pos, widget->mapToGlobal(pos),
                            Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(widget, &press);
        QApplication::sendEvent(widget, &release);
    };
    for (auto *box : {static_cast<QAbstractSpinBox *>(spin), static_cast<QAbstractSpinBox *>(fraction)}) {
        auto value = [box] {
            if (auto *integer = qobject_cast<QSpinBox *>(box)) return double(integer->value());
            return qobject_cast<QDoubleSpinBox *>(box)->value();
        };
        const double original = value();
        click(box, spinRect(box, QStyle::SC_SpinBoxUp).center());
        check(value() > original, "numeric up button still increments");
        click(box, spinRect(box, QStyle::SC_SpinBoxDown).center());
        check(value() == original, "numeric down button still decrements");
        box->setFocus();
        QKeyEvent upKey(QEvent::KeyPress, Qt::Key_Up, Qt::NoModifier);
        QApplication::sendEvent(box, &upKey);
        check(value() > original, "numeric arrow key still increments");
        QKeyEvent downKey(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier);
        QApplication::sendEvent(box, &downKey);
        check(value() == original, "numeric arrow key still decrements");
        auto *editor = box->findChild<QLineEdit *>();
        if (editor) {
            box->setKeyboardTracking(false);
            editor->selectAll();
            QKeyEvent digit(QEvent::KeyPress, Qt::Key_1, Qt::NoModifier, QStringLiteral("1"));
            QApplication::sendEvent(editor, &digit);
            QKeyEvent commit(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
            QApplication::sendEvent(box, &commit);
            check(value() == 1, "numeric typed value still commits");
        }
    }
    spin->setValue(250); fraction->setValue(.5625);
    spin->stepUp(); check(spin->value() == 251, "spin increments");
    check(!disabled->isEnabled(), "disabled combo stays disabled");
    QString renderDir;
    const auto args = app.arguments();
    const int renderIndex = args.indexOf(QStringLiteral("--render-dir"));
    if (renderIndex >= 0 && renderIndex + 1 < args.size()) {
        renderDir = args.at(renderIndex + 1); QDir().mkpath(renderDir);
        check(window.grab().save(renderDir + QStringLiteral("/controls.png")), "save controls preview");
    }
    for (auto *box : {static_cast<QAbstractSpinBox *>(spin), static_cast<QAbstractSpinBox *>(fraction)}) {
        box->setFocus(); app.processEvents();
        const auto preview = box->grab();
        const auto pixels = preview.toImage();
        const int buttonsStart = qRound(spinRect(box, QStyle::SC_SpinBoxUp).left() * preview.devicePixelRatio());
        int inputHighlight = 0, buttonHighlight = 0;
        for (int y = 0; y < pixels.height(); ++y) {
            for (int x = 0; x < pixels.width(); ++x) {
                const auto color = pixels.pixelColor(x, y);
                if (color.red() < 180 && color.green() > 210 && color.blue() > 180) {
                    if (x >= buttonsStart) ++buttonHighlight; else ++inputHighlight;
                }
            }
        }
        check(inputHighlight > 0, "focused numeric input retains visible mint outline");
        check(buttonHighlight == 0, "numeric buttons have no focus-highlight fragments");
        if (!renderDir.isEmpty()) {
            const auto name = box == spin ? QStringLiteral("/spin-focused.png") : QStringLiteral("/fraction-focused.png");
            check(preview.save(renderDir + name), "save focused numeric preview");
        }
    }
    combo->showPopup(); app.processEvents();
    check(combo->view()->isVisible(), "combo popup visible");
    if (!renderDir.isEmpty())
        check(combo->view()->window()->grab().save(renderDir + QStringLiteral("/dropdown.png")), "save dropdown preview");
    QKeyEvent next(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier);
    QApplication::sendEvent(combo->view(), &next);
    QKeyEvent accept(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QApplication::sendEvent(combo->view(), &accept);
    app.processEvents();
    check(combo->currentIndex() == 2, "popup keyboard selection");
    combo->hidePopup(); app.processEvents();
    check(!combo->view()->isVisible(), "combo popup closes");
    combo->showPopup(); app.processEvents();
    auto *popup = combo->view();
    auto *popupBar = popup->verticalScrollBar();
    popupBar->setValue(0);
    wheel(popup->viewport(), QPoint(0, -120));
    check(popupBar->value() > 0, "open dropdown list still scrolls with wheel");
    check(combo->currentIndex() == 2, "scrolling dropdown does not commit a selection");
    const auto clickIndex = combo->model()->index(5, 0);
    popup->scrollTo(clickIndex);
    const QPoint clickPos = popup->visualRect(clickIndex).center();
    QMouseEvent press(QEvent::MouseButtonPress, clickPos, popup->viewport()->mapToGlobal(clickPos),
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent release(QEvent::MouseButtonRelease, clickPos, popup->viewport()->mapToGlobal(clickPos),
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(popup->viewport(), &press);
    QApplication::sendEvent(popup->viewport(), &release);
    app.processEvents();
    check(combo->currentIndex() == 5, "mouse click still chooses dropdown option");
    combo->hidePopup();
    auto *bar = scroll->verticalScrollBar();
    auto *effect = qobject_cast<QGraphicsOpacityEffect *>(bar->graphicsEffect());
    check(effect != nullptr, "scrollbar has auto-hide effect");
    if (effect) {
        const auto viewportSize = scroll->viewport()->size();
        QEvent leave(QEvent::Leave);
        QApplication::sendEvent(bar, &leave);
        bar->setValue(81);
        check(effect->opacity() == 1, "scroll reveals scrollbar");
        wait(1550);
        check(effect->opacity() == 0, "idle scrollbar fades out");
        check(scroll->viewport()->size() == viewportSize, "hide does not reflow content");
        QEnterEvent enter(QPointF(2, 2), QPointF(2, 2), QPointF(2, 2));
        QApplication::sendEvent(bar, &enter);
        check(effect->opacity() == 1, "hidden scrollbar remains discoverable by hover");
        auto *idle = bar->findChild<QTimer *>(QStringLiteral("scrollbarIdleTimer"));
        check(idle != nullptr, "idle timer installed");
        if (idle) {
            QMetaObject::invokeMethod(idle, "timeout", Qt::DirectConnection);
            wait(200);
            check(effect->opacity() == 1, "hover prevents hiding");
            QApplication::sendEvent(bar, &leave);
            bar->setSliderDown(true);
            QMetaObject::invokeMethod(idle, "timeout", Qt::DirectConnection);
            wait(200);
            check(effect->opacity() == 1, "drag prevents hiding even outside scrollbar");
            bar->setSliderDown(false);
            QMetaObject::invokeMethod(idle, "timeout", Qt::DirectConnection);
            wait(200);
            check(effect->opacity() == 0, "release allows hiding again");
        }
        QApplication::sendEvent(scroll->viewport(), &enter);
        check(effect->opacity() == 1, "entering content reveals scrollbar");
        check(scroll->viewport()->size() == viewportSize, "reveal does not reflow content");
    }
    check(combo->view()->verticalScrollBar()->property("d4boxAutoHide").toBool(),
          "dropdown scrollbar uses the same auto-hide behavior");
    std::cout << "Theme controls: " << failures << " failures\n";
    return failures ? 1 : 0;
}
