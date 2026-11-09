#include "EditorTheme.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QAbstractItemView>
#include <QDir>
#include <QEnterEvent>
#include <QEventLoop>
#include <QGraphicsOpacityEffect>
#include <QTimer>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QStyledItemDelegate>
#include <QVBoxLayout>
#include <iostream>

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    EditorTheme::install(app);
    int failures = 0;
    auto check = [&](bool ok, const char *message) {
        if (!ok) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
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
    for (int i = 0; i < 20; ++i) {
        auto *label = new QLabel(QStringLiteral("动作 %1 · 等待界面就绪").arg(i + 1));
        label->setMinimumHeight(30); rows->addWidget(label);
    }
    scroll->setWidget(content); layout->addWidget(scroll);
    auto *save = new QPushButton(QStringLiteral("保存方案"));
    save->setProperty("role", QStringLiteral("primary")); layout->addWidget(save);
    window.show(); app.processEvents();
    check(scroll->verticalScrollBar()->maximum() > 0, "long content scrolls");
    scroll->verticalScrollBar()->setValue(80);
    check(scroll->verticalScrollBar()->value() == 80, "scrollbar can move");
    combo->setFocus();
    QKeyEvent down(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier);
    QApplication::sendEvent(combo, &down);
    check(combo->currentIndex() == 1, "combo keyboard navigation");
    spin->stepUp(); check(spin->value() == 251, "spin increments");
    check(!disabled->isEnabled(), "disabled combo stays disabled");
    QString renderDir;
    const auto args = app.arguments();
    const int renderIndex = args.indexOf(QStringLiteral("--render-dir"));
    if (renderIndex >= 0 && renderIndex + 1 < args.size()) {
        renderDir = args.at(renderIndex + 1); QDir().mkpath(renderDir);
        check(window.grab().save(renderDir + QStringLiteral("/controls.png")), "save controls preview");
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
    auto wait = [](int ms) {
        QEventLoop loop;
        QTimer::singleShot(ms, &loop, &QEventLoop::quit);
        loop.exec();
    };
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
