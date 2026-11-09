#include "EditorWindow.h"
#include "NativeOverlay.h"
#include "OverlayLayer.h"
#include "WorkflowPanel.h"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QImageReader>
#include <QInputDialog>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolBar>
#include <QUndoCommand>
#include <QUndoStack>
#include <QUuid>
#include <QWindow>

namespace {
class SnapshotCommand final : public QUndoCommand {
  public:
    SnapshotCommand(QString label, QJsonObject before, QJsonObject after,
                    std::function<void(const QJsonObject &)> apply)
        : QUndoCommand(std::move(label)), before(std::move(before)), after(std::move(after)),
          apply(std::move(apply)) {}
    void undo() override {
        apply(before);
    }
    void redo() override {
        apply(after);
    }

  private:
    QJsonObject before, after;
    std::function<void(const QJsonObject &)> apply;
};
QIcon appIcon() {
    QPixmap icon(64, 64);
    icon.fill(Qt::transparent);
    QPainter p(&icon);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(QColor(QStringLiteral("#20323F")));
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(QRectF(3, 3, 58, 58), 12, 12);
    p.setPen(QPen(QColor(QStringLiteral("#71F4DE")), 3));
    p.drawRect(QRectF(17, 17, 30, 30));
    p.drawLine(QPointF(32, 11), QPointF(32, 53));
    p.drawLine(QPointF(11, 32), QPointF(53, 32));
    return QIcon(icon);
}
} // namespace

EditorWindow::EditorWindow(WorkflowDocument input, QString folder, QImage image, int screen)
    : QMainWindow(nullptr, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint),
      document(std::move(input)), directory(std::move(folder)), backgroundMode(!image.isNull()),
      screenIndex(screen) {
    setObjectName(QStringLiteral("editorToolbar"));
    setWindowTitle(QStringLiteral("D4Box · 配置编辑器"));
    setWindowIcon(appIcon());
    setAttribute(Qt::WA_QuitOnClose, false);
#ifdef Q_OS_MACOS
    setAttribute(Qt::WA_MacAlwaysShowToolWindow, true);
#endif
    qApp->installEventFilter(this);
    qApp->setStyleSheet(
        QStringLiteral("QWidget { color:#F0F5FA; font-size:13px; }"
                       "QMainWindow#editorToolbar, QToolBar { background:#20323F; border:0; spacing:6px; }"
                       "QWidget#stepsPanel, QDialog, QMessageBox, QTabWidget::pane { background:#111A23; }"
                       "QScrollArea, QScrollArea > QWidget > QWidget { background:#111A23; }"
                       "QPushButton, QToolButton { background:#1B2833; border:1px solid #43515D; "
                       "border-radius:5px; padding:6px 9px; }"
                       "QPushButton:hover, QToolButton:hover { border-color:#71F4DE; background:#233F39; }"
                       "QPushButton:focus, QLineEdit:focus, QComboBox:focus, QSpinBox:focus, "
                       "QDoubleSpinBox:focus { border:1px solid #71F4DE; }"
                       "QPushButton[danger=true] { color:#EDB6B6; border-color:#80585D; }"
                       "QPushButton:disabled, QToolButton:disabled { color:#73818C; border-color:#30414F; }"
                       "QComboBox, QLineEdit, QSpinBox, QDoubleSpinBox { background:#111A23; border:1px "
                       "solid #43515D; border-radius:4px; padding:5px; min-height:20px; }"
                       "QComboBox QAbstractItemView, QListWidget { background:#1B2833; "
                       "selection-background-color:#233F39; color:#F0F5FA; }"
                       "QListWidget { border:1px solid #43515D; border-radius:5px; }"
                       "QListWidget::item { padding:8px; border-bottom:1px solid #30414F; }"
                       "QListWidget::item:selected { background:#233F39; border:1px solid #71F4DE; }"
                       "QTabBar::tab { background:#20323F; padding:9px 20px; }"
                       "QTabBar::tab:selected { background:#233F39; border-bottom:2px solid #71F4DE; }"
                       "QMenu { background:#111A23; border:1px solid #43515D; padding:5px; }"
                       "QMenu::item { padding:7px 22px; } QMenu::item:selected { background:#233F39; }"
                       "QLabel#validation { color:#BCCAD6; font-size:12px; }"
                       "QToolTip { color:#F0F5FA; background:#20323F; border:1px solid #71F4DE; }"));
    undo = new QUndoStack(this);
    undo->setUndoLimit(200);
    overlay = new OverlayLayer(&document, screenIndex, image, this);
    shortcuts = new NativeShortcuts(this);
    panel = new WorkflowPanel(
        &document, [this](const QString &label, const std::function<void()> &edit) { mutate(label, edit); },
        this);
#ifdef Q_OS_MACOS
    panel->setAttribute(Qt::WA_MacAlwaysShowToolWindow, true);
#endif
    panel->locatePoint = [this](const QString &id) { selectPoint(id); };
    panel->cropChanged = [this](const QRectF &rect) {
        referenceCrop = rect;
        overlay->setReferenceRect(rect);
        invalidateReference();
    };
    panel->confirmReference = [this] {
        if (QMessageBox::question(panel, QStringLiteral("人工确认参考区"),
                                  QStringLiteral("请确认有效区域已排除标题栏和黑边，点位及背包与当前界面吻合"
                                                 "。\n这只记录参考尺寸，不代表游戏验证成功。"),
                                  QMessageBox::Yes | QMessageBox::Cancel,
                                  QMessageBox::Cancel) != QMessageBox::Yes)
            return;
        mutate(QStringLiteral("人工确认参考尺寸"), [this] {
            const auto size = overlay->referencePixelSize();
            auto l = document.layout();
            l.insert(QStringLiteral("referenceClientPx"),
                     QJsonObject{{QStringLiteral("width"), size.width()},
                                 {QStringLiteral("height"), size.height()}});
            l.insert(QStringLiteral("calibration"), QStringLiteral("manual"));
            document.setLayout(l);
        });
    };
    bar = new QToolBar(this);
    bar->setMovable(false);
    bar->setContentsMargins(6, 4, 6, 4);
    bar->setToolButtonStyle(Qt::ToolButtonTextOnly);
    addToolBar(bar);
    brand = new QLabel(QStringLiteral("  D4BOX  "));
    brand->setCursor(Qt::SizeAllCursor);
    brand->setStyleSheet(QStringLiteral("color:#71F4DE;font-weight:700"));
    brand->setToolTip(QStringLiteral("拖动此处移动工具栏"));
    brand->installEventFilter(this);
    bar->addWidget(brand);
    profiles = new QComboBox;
    profiles->setFixedWidth(175);
    profiles->setAccessibleName(QStringLiteral("同目录配置"));
    bar->addWidget(profiles);
    name = new QLineEdit;
    name->setFixedWidth(142);
    name->setAccessibleName(QStringLiteral("配置名称"));
    bar->addWidget(name);
    status = new QLabel;
    status->setFixedWidth(16);
    bar->addWidget(status);
    auto action = [this](const QString &text, const std::function<void()> &run) {
        auto *a = bar->addAction(text);
        connect(a, &QAction::triggered, this, run);
        return a;
    };
    action(QStringLiteral("＋ 点位"), [this] {
        QString id;
        mutate(QStringLiteral("新增点位"), [this, &id] {
            id = document.addPoint(QStringLiteral("点位 %1").arg(
                                       document.data.value(QStringLiteral("points")).toArray().size() + 1),
                                   QPointF(.5, .5));
        });
        panel->showPoints(id);
        selectPoint(id);
    });
    action(QStringLiteral("背包"), [this] {
        showSteps();
        panel->showLayout();
    });
    action(QStringLiteral("编辑步骤"), [this] { showSteps(); });
    action(QStringLiteral("预览"), [this] {
        togglePreview();
    })->setToolTip(QStringLiteral("仅点位和背包格线；Ctrl+Alt+D 返回。不是实时物品识别。"));
    operate = action(QStringLiteral("操作底层"), [this] {
        const bool passthrough = operate->isChecked();
        overlay->setEditingVisible(!passthrough);
        overlay->setDrawingVisible(!passthrough);
        panel->hide();
        operate->setText(passthrough ? QStringLiteral("返回编辑") : QStringLiteral("操作底层"));
    });
    operate->setCheckable(true);
    action(QStringLiteral("保存"), [this] { save(); });
    auto *more = action(QStringLiteral("更多"), [] {});
    auto *menu = new QMenu(this);
    more->setMenu(menu);
    auto option = [this, menu](const QString &label, const std::function<void()> &fn) {
        auto *a = menu->addAction(label);
        connect(a, &QAction::triggered, this, fn);
        return a;
    };
    option(QStringLiteral("新建空白配置"), [this] { newDocument(); });
    option(QStringLiteral("从自动嬗变模板新建（待校准）"), [this] { newDocument(true); });
    option(QStringLiteral("打开文件 / 刷新目录"), [this] { openDocument(); });
    option(QStringLiteral("复制为新配置"), [this] {
        auto next = WorkflowDocument::createDraft();
        next.data = document.data;
        next.data.insert(QStringLiteral("id"), QUuid::createUuid().toString(QUuid::WithoutBraces));
        next.data.insert(QStringLiteral("revision"), 0);
        next.data.insert(QStringLiteral("status"), QStringLiteral("draft"));
        next.setName(next.data.value(QStringLiteral("name")).toString() + QStringLiteral(" · 副本"));
        changeDocument(std::move(next), directory);
    });
    option(QStringLiteral("另存为新配置…"), [this] { save(true); });
    option(QStringLiteral("保存并退出"), [this] {
        if (save())
            close();
    });
    menu->addSeparator();
    menu->addAction(undo->createUndoAction(this, QStringLiteral("撤销")));
    menu->addAction(undo->createRedoAction(this, QStringLiteral("重做")));
    option(QStringLiteral("校验结构与静态前提"), [this] { showValidation(); });
    option(QStringLiteral("隐藏编辑控件 · Ctrl+Alt+P"), [this] { toggleControls(); });
    option(QStringLiteral("更换本地底图…"), [this] {
        const auto file = QFileDialog::getOpenFileName(this, QStringLiteral("仅作为本地测试底图"), {},
                                                       QStringLiteral("图片 (*.png *.jpg *.jpeg)"));
        if (file.isEmpty())
            return;
        QImageReader reader(file);
        reader.setAutoTransform(true);
        const auto image = reader.read();
        if (image.isNull()) {
            QMessageBox::warning(this, QStringLiteral("读取失败"), reader.errorString());
            return;
        }
        overlay->setBackground(image);
        updateViewIdentity(image);
        loadViewState();
        backgroundMode = true;
        invalidateReference();
    });
    option(QStringLiteral("移除底图 · 覆盖真实桌面"), [this] {
        overlay->setBackground({});
        updateViewIdentity({});
        loadViewState();
        backgroundMode = false;
        invalidateReference();
    });
    auto *screensMenu = menu->addMenu(QStringLiteral("选择显示器"));
    const auto screens = QGuiApplication::screens();
    for (int i = 0; i < screens.size(); ++i) {
        auto *a = screensMenu->addAction(QStringLiteral("%1 · %2").arg(i).arg(screens[i]->name()));
        connect(a, &QAction::triggered, this, [this, i] {
            screenIndex = i;
            overlay->setScreenIndex(i);
            if (!backgroundMode)
                updateViewIdentity({});
            loadViewState();
            invalidateReference();
            placeToolbar();
        });
    }
    option(QStringLiteral("快捷键与边界说明"), [this] {
        QMessageBox::information(
            this, QStringLiteral("编辑器快捷键"),
            QStringLiteral(
                "Ctrl+Alt+P（Mac: "
                "Control+Option+P）：召回/隐藏控件\nCtrl+Alt+D：切换只读预览\nCtrl/Cmd+S：保存草稿\nCtrl/"
                "Cmd+Z：撤销；Shift+Ctrl/Cmd+Z：重做\n双击点位：改名；拖背包边框：移动；8 "
                "个柄：缩放\n\n只编辑 JSON，不点击游戏、不识别物品；退出前保存或放弃修改。\n") +
                (shortcuts->active() ? QStringLiteral("全局编辑快捷键已注册。") : shortcuts->error()));
    });
    action(QStringLiteral("退出"), [this] { close(); });
    auto *undoAction = undo->createUndoAction(this);
    undoAction->setShortcut(QKeySequence::Undo);
    undoAction->setShortcutContext(Qt::ApplicationShortcut);
    addAction(undoAction);
    auto *redoAction = undo->createRedoAction(this);
    redoAction->setShortcut(QKeySequence::Redo);
    redoAction->setShortcutContext(Qt::ApplicationShortcut);
    addAction(redoAction);
    // The menu actions live in a popup; these window actions work while editing fields too.
    auto *saveAction = new QAction(this);
    saveAction->setShortcut(QKeySequence::Save);
    saveAction->setShortcutContext(Qt::ApplicationShortcut);
    addAction(saveAction);
    connect(saveAction, &QAction::triggered, this, [this] { save(); });
    connect(name, &QLineEdit::textEdited, this, [this](const QString &text) {
        mutate(QStringLiteral("重命名配置"), [this, text] { document.setName(text); });
    });
    connect(profiles, &QComboBox::activated, this, [this](int i) {
        const auto path = profiles->itemData(i).toString();
        if (path.isEmpty() || path == document.filePath || !mayDiscard()) {
            refreshCatalog();
            return;
        }
        WorkflowDocument next;
        QString error;
        if (!next.load(path, &error)) {
            QMessageBox::warning(this, QStringLiteral("读取失败"), error);
            refreshCatalog();
            return;
        }
        changeDocument(std::move(next), QFileInfo(path).absolutePath());
    });
    connect(undo, &QUndoStack::indexChanged, this, [this] {
        document.dirty = document.filePath.isEmpty() || !undo->isClean();
        refresh();
    });
    connect(overlay, &OverlayLayer::editStarted, this, [this] {
        dragging = true;
        dragSnapshot = document.data;
        panelBeforeDrag = panel->isVisible();
        controlsBeforeDrag = isVisible();
        panel->hide();
        hide();
    });
    connect(overlay, &OverlayLayer::geometryEdited, this, [this] { status->setText(QStringLiteral("●")); });
    connect(overlay, &OverlayLayer::editFinished, this, [this] {
        dragging = false;
        if (dragSnapshot != document.data)
            undo->push(new SnapshotCommand(QStringLiteral("拖动布局"), dragSnapshot, document.data,
                                           [this](const QJsonObject &s) { applySnapshot(s); }));
        dragSnapshot = {};
        if (controlsBeforeDrag && !preview) {
            if (placeToolbar(true))
                show();
        }
        if (panelBeforeDrag && !preview) {
            bool blocked = false;
            for (auto *w : overlay->interactiveWindows())
                blocked |= w->isVisible() && w->geometry().intersects(panel->geometry());
            if (!blocked)
                panel->show();
        }
        refresh();
    });
    connect(overlay, &OverlayLayer::pointActivated, this,
            [this](const QString &id) { panel->showPoints(id); });
    connect(overlay, &OverlayLayer::pointEditRequested, this, [this](const QString &id) {
        QString current;
        for (const auto &p : document.data.value(QStringLiteral("points")).toArray())
            if (p.toObject().value(QStringLiteral("id")) == id)
                current = p.toObject().value(QStringLiteral("name")).toString();
        bool ok = false;
        const auto text =
            QInputDialog::getText(this, QStringLiteral("点位名称"), QStringLiteral("名称不改变点位 ID"),
                                  QLineEdit::Normal, current, &ok);
        if (ok)
            mutate(QStringLiteral("重命名点位"), [this, id, text] { document.renamePoint(id, text); });
    });
    connect(overlay, &OverlayLayer::restoreRequested, this, [this] { restoreControls(); });
    connect(overlay, &OverlayLayer::screenChanged, this, [this] {
        if (!dragging) {
            placeToolbar();
            invalidateReference();
        }
    });
    connect(shortcuts, &NativeShortcuts::toggleControls, this, [this] { toggleControls(); });
    connect(shortcuts, &NativeShortcuts::togglePreview, this, [this] { togglePreview(); });
    tray = new QSystemTrayIcon(appIcon(), this);
    tray->setToolTip(QStringLiteral("D4Box 配置编辑器 · 不执行游戏操作"));
    auto *trayMenu = new QMenu(this);
    auto *restore = trayMenu->addAction(QStringLiteral("返回编辑 / 召回工具栏"));
    connect(restore, &QAction::triggered, this, [this] { restoreControls(); });
    auto *quit = trayMenu->addAction(QStringLiteral("退出编辑器…"));
    connect(quit, &QAction::triggered, this, [this] {
        restoreControls();
        close();
    });
    tray->setContextMenu(trayMenu);
    connect(tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
            restoreControls();
    });
    if (QSystemTrayIcon::isSystemTrayAvailable())
        tray->show();
    updateViewIdentity(image);
    loadViewState();
    refreshCatalog();
    refresh();
}
EditorWindow::~EditorWindow() {
    qApp->removeEventFilter(this);
    disconnect(overlay, nullptr, this, nullptr);
    disconnect(undo, nullptr, this, nullptr);
    undo->clear();
    overlay->hide();
    tray->hide();
    // Borrowed-document users must die BEFORE the document member, not in QObject base teardown.
    delete overlay;
    delete panel;
    delete shortcuts;
}

void EditorWindow::start() {
    overlay->show();
    placeToolbar();
    show();
    raise();
    if (!shortcuts->active())
        status->setToolTip(shortcuts->error() +
                           QStringLiteral("\n全隐藏模式将在 10 秒后恢复，亦可从托盘召回。"));
}
void EditorWindow::mutate(const QString &label, const std::function<void()> &edit) {
    const auto before = document.data;
    edit();
    document.refreshRequirements();
    const auto after = document.data;
    if (before != after)
        undo->push(
            new SnapshotCommand(label, before, after, [this](const QJsonObject &s) { applySnapshot(s); }));
}
void EditorWindow::applySnapshot(const QJsonObject &snapshot) {
    // Revision belongs to successful disk writes, not undo history.
    const auto revision = document.data.value(QStringLiteral("revision"));
    document.data = snapshot;
    document.data.insert(QStringLiteral("revision"), revision);
    document.dirty = true;
    overlay->refresh();
    panel->refresh();
    refresh();
}
void EditorWindow::refresh() {
    if (name->text() != document.data.value(QStringLiteral("name")).toString()) {
        QSignalBlocker b(name);
        name->setText(document.data.value(QStringLiteral("name")).toString());
    }
    status->setText(document.dirty ? QStringLiteral("●") : QStringLiteral("✓"));
    status->setToolTip(document.dirty ? QStringLiteral("未保存修改 · 只写草稿 JSON")
                                      : QStringLiteral("已保存草稿 · 不执行"));
    setWindowTitle(QStringLiteral("D4Box · %1%2")
                       .arg(document.data.value(QStringLiteral("name")).toString(),
                            document.dirty ? QStringLiteral(" *") : QString{}));
    if (!dragging)
        panel->refresh();
}
void EditorWindow::refreshCatalog() {
    QSignalBlocker b(profiles);
    profiles->clear();
    if (document.filePath.isEmpty())
        profiles->addItem(QStringLiteral("新配置 · 未保存"), QString{});
    QStringList invalid;
    QHash<QString, int> ids;
    const auto files = WorkflowDocument::catalog(directory, &invalid);
    for (const auto &path : files) {
        WorkflowDocument item;
        QString error;
        if (item.load(path, &error))
            ++ids[item.data.value(QStringLiteral("id")).toString()];
    }
    for (const auto &path : files) {
        WorkflowDocument item;
        QString error;
        item.load(path, &error);
        const bool duplicate = ids.value(item.data.value(QStringLiteral("id")).toString()) > 1;
        profiles->addItem((duplicate ? QStringLiteral("[ID 冲突] ") : QString{}) + QFileInfo(path).fileName(),
                          path);
    }
    int index = profiles->findData(document.filePath);
    if (index < 0 && !document.filePath.isEmpty()) {
        profiles->addItem(QFileInfo(document.filePath).fileName(), document.filePath);
        index = profiles->count() - 1;
    }
    profiles->setCurrentIndex(qMax(0, index));
    profiles->setToolTip(invalid.isEmpty() ? QStringLiteral("同目录配置；按文件名排序")
                                           : invalid.join(QLatin1Char('\n')));
}
void EditorWindow::showSteps() {
    if (preview)
        restoreControls();
    if (!panel->property("positioned").toBool()) {
        const auto screen = QGuiApplication::screens().value(screenIndex, QGuiApplication::primaryScreen());
        const auto available = screen->availableGeometry();
        panel->resize(qMin(438, available.width() - 24), qMin(720, available.height() - 88));
        panel->move(available.left() + 24, available.top() + 70);
        panel->setProperty("positioned", true);
    }
    panel->show();
    panel->raise();
    panel->activateWindow();
}
bool EditorWindow::placeToolbar(bool avoidPoints) {
    auto *screen = QGuiApplication::screens().value(screenIndex, QGuiApplication::primaryScreen());
    if (!screen)
        return false;
    const auto r = screen->availableGeometry();
    setFixedSize(qMin(1190, r.width() - 24), 52);
    const int left = r.center().x() - width() / 2;
    if (avoidPoints) {
        for (int y = r.top() + 8; y + height() < r.bottom() - 4; y += height() + 8) {
            const QRect candidate(QPoint(left, y), size());
            bool blocked = false;
            for (auto *w : overlay->interactiveWindows())
                blocked |= w->isVisible() && candidate.intersects(w->geometry().adjusted(-5, -5, 5, 5));
            if (!blocked) {
                move(candidate.topLeft());
                return true;
            }
        }
        // No clear strip: stay hidden after a drag rather than cover the dropped target.
        // The registered global shortcut/tray can explicitly summon controls again.
        return false;
    }
    move(left, r.top() + 8);
    return true;
}

void EditorWindow::selectPoint(const QString &id) {
    if (id.isEmpty())
        return;
    overlay->selectPoint(id);
    panel->showPoints(id);
    panel->hide();
    if (!placeToolbar(true))
        hide();
}
void EditorWindow::invalidateReference() {
    // Reference/crop/background changes are view changes, not shareable file coordinates.
    // Start a new undo epoch: otherwise Undo could resurrect "manual" calibration against
    // a different screen or image. Keep all actual unsaved document edits and dirty state.
    const bool wasDirty = document.dirty;
    const auto before = document.data;
    auto layout = document.layout();
    layout.insert(QStringLiteral("calibration"), QStringLiteral("unverified"));
    layout.insert(QStringLiteral("referenceClientPx"), QJsonValue::Null);
    document.setLayout(layout);
    const bool changed = before != document.data;
    undo->clear();
    if (wasDirty || changed)
        undo->resetClean();
    document.dirty = document.filePath.isEmpty() || !undo->isClean();
    refresh();
}

void EditorWindow::restoreControls() {
    preview = false;
    controlsVisible = true;
    operate->setChecked(false);
    operate->setText(QStringLiteral("操作底层"));
    overlay->setReadOnly(false);
    overlay->setDrawingVisible(true);
    overlay->setEditingVisible(true);
    overlay->show();
    placeToolbar(true);
    show();
    raise();
    activateWindow();
}
void EditorWindow::toggleControls() {
    if (preview || !isVisible()) {
        restoreControls();
        return;
    }
    controlsVisible = false;
    hide();
    panel->hide();
    if (!shortcuts->active())
        QTimer::singleShot(10000, this, [this] {
            if (!isVisible())
                restoreControls();
        });
}
void EditorWindow::togglePreview() {
    if (preview) {
        restoreControls();
        return;
    }
    preview = true;
    overlay->setDrawingVisible(true);
    overlay->setReadOnly(true);
    hide();
    panel->hide();
    if (!shortcuts->active())
        QTimer::singleShot(10000, this, [this] {
            if (preview)
                restoreControls();
        });
}
void EditorWindow::changeDocument(WorkflowDocument next, QString folder) {
    document = std::move(next);
    directory = std::move(folder);
    undo->clear();
    if (!document.dirty)
        undo->setClean();
    name->clearFocus();
    if (!document.filePath.isEmpty())
        loadViewState();
    refreshCatalog();
    overlay->refresh();
    refresh();
}
void EditorWindow::newDocument(bool useTemplate) {
    if (!mayDiscard())
        return;
    changeDocument(useTemplate ? WorkflowDocument::createTransmuteDraft() : WorkflowDocument::createDraft(),
                   directory);
}
void EditorWindow::openDocument() {
    if (!mayDiscard())
        return;
    const auto path = QFileDialog::getOpenFileName(this, QStringLiteral("打开配置 JSON"), directory,
                                                   QStringLiteral("流程 JSON (*.json)"));
    if (path.isEmpty()) {
        refreshCatalog();
        return;
    }
    WorkflowDocument next;
    QString error;
    if (!next.load(path, &error)) {
        QMessageBox::warning(this, QStringLiteral("读取失败"), error);
        return;
    }
    changeDocument(std::move(next), QFileInfo(path).canonicalPath());
}
void EditorWindow::showValidation() {
    const auto structural = document.issues();
    const auto ready = document.issues(true);
    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("仅静态校验 · 不执行"));
    box.setText(structural.isEmpty() ? QStringLiteral("JSON 结构和引用有效")
                                     : QStringLiteral("结构存在 %1 个问题").arg(structural.size()));
    box.setInformativeText(ready.isEmpty()
                               ? QStringLiteral("静态前提完整；仍保存为 draft，执行器未接入，未经游戏验证。")
                               : QStringLiteral("静态就绪检查还剩 %1 项。可保存结构合法的草稿。\n%2")
                                     .arg(ready.size())
                                     .arg(ready.mid(0, 5).join(QLatin1Char('\n'))));
    box.setDetailedText(ready.join(QLatin1Char('\n')));
    box.exec();
}
bool EditorWindow::save(bool saveAs) {
    // Flush spin boxes/line editors before constructing the disk snapshot.
    if (auto *focused = QApplication::focusWidget())
        focused->clearFocus();
    const auto errors = document.issues();
    if (!errors.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("无法保存非法结构"),
                             errors.mid(0, 8).join(QLatin1Char('\n')));
        return false;
    }
    QString path = saveAs ? QString{} : document.filePath;
    if (path.isEmpty())
        path = QFileDialog::getSaveFileName(this, QStringLiteral("保存草稿 JSON（不执行）"),
                                            QDir(directory.isEmpty() ? QDir::homePath() : directory)
                                                .filePath(QStringLiteral("workflow.d4workflow.json")),
                                            QStringLiteral("流程 JSON (*.json)"));
    if (path.isEmpty())
        return false;
    if (QFileInfo(path).suffix().isEmpty())
        path += QStringLiteral(".d4workflow.json");
    if (document.data.value(QStringLiteral("status")) == QStringLiteral("ready") && !saveAs &&
        QMessageBox::warning(
            this, QStringLiteral("将保存为草稿"),
            QStringLiteral("编辑器尚未接执行器，此文件会保存为 draft 而不是 ready。继续覆盖吗？"),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes)
        return false;
    const bool different =
        QFileInfo(path).absoluteFilePath() != QFileInfo(document.filePath).absoluteFilePath();
    bool overwrite = false;
    if (different && QFileInfo::exists(path)) {
        if (QMessageBox::warning(this, QStringLiteral("覆盖已有文件"),
                                 QStringLiteral("目标文件已存在。确认替换？\n%1").arg(path),
                                 QMessageBox::Yes | QMessageBox::Cancel,
                                 QMessageBox::Cancel) != QMessageBox::Yes)
            return false;
        overwrite = true;
    }
    QString error;
    if (saveAs) {
        auto copy = WorkflowDocument::createDraft();
        copy.data = document.data;
        copy.data.insert(QStringLiteral("id"), QUuid::createUuid().toString(QUuid::WithoutBraces));
        copy.data.insert(QStringLiteral("revision"), 0);
        if (!copy.saveDraft(path, &error, overwrite)) {
            QMessageBox::warning(this, QStringLiteral("保存失败"), error);
            return false;
        }
        changeDocument(std::move(copy), QFileInfo(path).absolutePath());
    } else if (!document.saveDraft(path, &error, overwrite)) {
        QMessageBox::warning(this, QStringLiteral("保存失败 · 修改仍在内存"), error);
        return false;
    }
    directory = QFileInfo(document.filePath).absolutePath();
    if (!saveViewState())
        QMessageBox::warning(
            this, QStringLiteral("JSON 已保存"),
            QStringLiteral("配置已安全写入；本机裁剪记忆无法写入，下次打开请重新设置参考区。"));
    undo->setClean();
    document.dirty = false;
    refreshCatalog();
    refresh();
    return true;
}
bool EditorWindow::mayDiscard() {
    if (auto *focused = QApplication::focusWidget())
        focused->clearFocus();
    if (!document.dirty)
        return true;
    auto response = QMessageBox::question(
        this, QStringLiteral("未保存修改"),
        QStringLiteral("保存“%1”的修改？放弃后未保存内容会丢失。\n保存仅写入草稿，不执行游戏操作。")
            .arg(document.data.value(QStringLiteral("name")).toString()),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel);
    if (response == QMessageBox::Save)
        return save();
    return response == QMessageBox::Discard;
}
void EditorWindow::closeEvent(QCloseEvent *event) {
    if (!diagnosticClosing && !exitApproved && !mayDiscard()) {
        event->ignore();
        return;
    }
    exitApproved = true;
    overlay->hide();
    panel->hide();
    tray->hide();
    event->accept();
    QTimer::singleShot(0, qApp, &QCoreApplication::quit);
}
void EditorWindow::closePreview() {
    diagnosticClosing = true;
    document.dirty = false;
    close();
}
bool EditorWindow::eventFilter(QObject *watched, QEvent *event) {
    if (watched == qApp && event->type() == QEvent::Quit && !diagnosticClosing && !exitApproved) {
        if (!mayDiscard())
            return true;
        exitApproved = true;
    }
    if (watched == brand) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *m = static_cast<QMouseEvent *>(event);
            if (m->button() == Qt::LeftButton) {
                movingToolbar = true;
                toolbarOffset = m->globalPosition().toPoint() - pos();
                return true;
            }
        }
        if (event->type() == QEvent::MouseMove && movingToolbar) {
            auto *m = static_cast<QMouseEvent *>(event);
            move(m->globalPosition().toPoint() - toolbarOffset);
            return true;
        }
        if (event->type() == QEvent::MouseButtonRelease) {
            movingToolbar = false;
            return true;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}
QImage EditorWindow::renderPreview(bool showPanel) {
    QImage output = overlay->renderPreview(!preview);
    if (output.isNull())
        return output;
    if (showPanel && !preview)
        showSteps();
    QPainter p(&output);
    const auto screen = QGuiApplication::screens().value(screenIndex, QGuiApplication::primaryScreen());
    const auto r = screen->geometry();
    p.scale(double(output.width()) / r.width(), double(output.height()) / r.height());
    if (!preview) {
        p.drawPixmap(pos() - r.topLeft(), grab());
        if (showPanel)
            p.drawPixmap(panel->pos() - r.topLeft(), panel->grab());
    }
    return output;
}
void EditorWindow::updateViewIdentity(const QImage &background) {
    if (!background.isNull()) {
        const QImage canonical = background.convertToFormat(QImage::Format_RGBA8888);
        const QByteArrayView bytes(reinterpret_cast<const char *>(canonical.constBits()),
                                   canonical.sizeInBytes());
        viewIdentity =
            QStringLiteral("image:") +
            QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
    } else {
        const auto *screen = QGuiApplication::screens().value(screenIndex, QGuiApplication::primaryScreen());
        viewIdentity = screen ? QStringLiteral("screen:%1:%2:%3:%4")
                                    .arg(screen->name())
                                    .arg(screen->geometry().width())
                                    .arg(screen->geometry().height())
                                    .arg(screen->devicePixelRatio())
                              : QStringLiteral("screen:none");
    }
}
QString EditorWindow::viewStateKey() const {
    const auto key =
        (document.data.value(QStringLiteral("id")).toString() + QLatin1Char('/') +
         document.data.value(QStringLiteral("defaultLayoutId")).toString() + QLatin1Char('/') + viewIdentity)
            .toUtf8();
    return QStringLiteral("reference/") +
           QString::fromLatin1(QCryptographicHash::hash(key, QCryptographicHash::Sha256).toHex());
}
void EditorWindow::loadViewState(const QString &file) {
    if (document.filePath.isEmpty() && file.isEmpty())
        return;
    const auto path = file.isEmpty()
                          ? QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
                                .filePath(QStringLiteral("editor-views.ini"))
                          : file;
    QSettings settings(path, QSettings::IniFormat);
    const QRectF saved = settings.value(viewStateKey()).toRectF();
    if (saved.width() < .05 || saved.height() < .05 || !QRectF(0, 0, 1, 1).contains(saved))
        return;
    referenceCrop = saved;
    overlay->setReferenceRect(saved);
    panel->setReferenceCrop(saved);
}
bool EditorWindow::saveViewState(const QString &file) const {
    const auto path = file.isEmpty()
                          ? QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
                                .filePath(QStringLiteral("editor-views.ini"))
                          : file;
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return false;
    QSettings settings(path, QSettings::IniFormat);
    settings.setValue(viewStateKey(), referenceCrop);
    settings.sync();
    return settings.status() == QSettings::NoError;
}
QJsonObject EditorWindow::diagnostics() const {
    auto result = overlay->diagnostics();
    result.insert(QStringLiteral("globalShortcuts"), shortcuts->active());
    result.insert(QStringLiteral("shortcutError"), shortcuts->error());
    result.insert(QStringLiteral("documentStatus"), document.data.value(QStringLiteral("status")));
    result.insert(QStringLiteral("editorOnly"), true);
    return result;
}
QJsonObject EditorWindow::selfTest() {
    QJsonArray passed, failed;
    auto check = [&](bool yes, const QString &label) { (yes ? passed : failed).append(label); };
    check(!panel->isVisible(), QStringLiteral("panel initially hidden"));
    check(overlay->drawingWindow()->windowFlags().testFlag(Qt::WindowTransparentForInput),
          QStringLiteral("drawing window input-transparent"));
    const auto startData = document.data;
    QString id;
    mutate(QStringLiteral("test add"),
           [this, &id] { id = document.addPoint(QStringLiteral("Test point"), QPointF(.5, .5)); });
    check(document.data.value(QStringLiteral("points")).toArray().size() ==
              startData.value(QStringLiteral("points")).toArray().size() + 1,
          QStringLiteral("point add"));
    undo->undo();
    check(document.data.value(QStringLiteral("points")) == startData.value(QStringLiteral("points")),
          QStringLiteral("undo point add"));
    undo->redo();
    check(document.layout().value(QStringLiteral("pointPositions")).toObject().contains(id),
          QStringLiteral("redo point reference"));
    showSteps();
    check(panel->isVisible(), QStringLiteral("panel open"));
    auto *add = panel->findChild<QPushButton *>(QStringLiteral("addStep"));
    const int before = document.data.value(QStringLiteral("steps")).toArray().size();
    if (add && add->menu() && !add->menu()->actions().isEmpty())
        add->menu()->actions().first()->trigger();
    check(document.data.value(QStringLiteral("steps")).toArray().size() == before + 1,
          QStringLiteral("add click step through UI"));
    if (add)
        add->menu()->actions().at(2)->trigger();
    auto *list = panel->findChild<QListWidget *>(QStringLiteral("stepsList"));
    auto *up = panel->findChild<QPushButton *>(QStringLiteral("stepUp"));
    const auto selected =
        list->currentItem() ? list->currentItem()->data(Qt::UserRole).toString() : QString{};
    if (up)
        up->click();
    check(document.data.value(QStringLiteral("steps"))
                  .toArray()
                  .at(qMax(0, before))
                  .toObject()
                  .value(QStringLiteral("id")) == selected,
          QStringLiteral("reorder step through UI"));
    panel->hide();
    showSteps();
    check(list->count() == before + 2, QStringLiteral("panel close retains steps"));
    mutate(QStringLiteral("test resize"), [this] { document.setInventoryRect(QRectF(.2, .3, .6, .2)); });
    check(document.layout()
                  .value(QStringLiteral("inventory"))
                  .toObject()
                  .value(QStringLiteral("rect"))
                  .toObject()
                  .value(QStringLiteral("width")) == .6,
          QStringLiteral("inventory resize persisted"));
    togglePreview();
    bool anyVisible = false;
    for (auto *w : overlay->interactiveWindows())
        anyVisible |= w->isVisible();
    check(!isVisible() && !panel->isVisible() && !anyVisible,
          QStringLiteral("readonly hides toolbar panel and all handles"));
    restoreControls();
    check(isVisible(), QStringLiteral("restore editing controls"));
    check(document.issues().isEmpty(), QStringLiteral("edited draft structurally valid"));
    auto *actionBox = panel->findChild<QComboBox *>(QStringLiteral("stepAction"));
    for (int index : {3, 0, 2, 1}) {
        actionBox->setCurrentIndex(index);
        QMetaObject::invokeMethod(actionBox, "activated", Qt::DirectConnection, Q_ARG(int, index));
        check(document.issues().isEmpty(),
              QStringLiteral("switch action %1 maintains valid schema").arg(index));
    }
    auto *source = panel->findChild<QComboBox *>(QStringLiteral("sourceMode"));
    for (int index : {1, 2, 0}) {
        source->setCurrentIndex(index);
        QMetaObject::invokeMethod(source, "activated", Qt::DirectConnection, Q_ARG(int, index));
        check(document.issues().isEmpty(), QStringLiteral("source mode %1 bounded configuration").arg(index));
    }
    // Text edits must stay dirty even when incomplete, so Save cannot silently drop them.
    auto *skip = panel->findChild<QLineEdit *>(QStringLiteral("excludedSlots"));
    skip->setText(QStringLiteral("99"));
    QMetaObject::invokeMethod(skip, "textEdited", Qt::DirectConnection, Q_ARG(QString, QStringLiteral("99")));
    check(!document.issues().isEmpty(), QStringLiteral("invalid exclusion kept and blocks save"));
    skip->setText(QStringLiteral("1, 33"));
    QMetaObject::invokeMethod(skip, "textEdited", Qt::DirectConnection,
                              Q_ARG(QString, QStringLiteral("1, 33")));
    check(document.data.value(QStringLiteral("source")).toObject().value(QStringLiteral("excludedSlots")) ==
              QJsonArray{0, 32},
          QStringLiteral("one-based UI slots serialize zero-based"));
    auto *addCheck = panel->findChild<QPushButton *>(QStringLiteral("addCheck"));
    addCheck->click();
    check(document.issues().isEmpty(), QStringLiteral("uncalibrated check saves as draft"));
    auto *color = panel->findChild<QLineEdit *>(QStringLiteral("checkColor"));
    color->setText(QStringLiteral("#invalid"));
    QMetaObject::invokeMethod(color, "textEdited", Qt::DirectConnection,
                              Q_ARG(QString, QStringLiteral("#invalid")));
    check(!document.issues().isEmpty(), QStringLiteral("invalid color retained and rejected"));
    color->setText(QStringLiteral("#123456"));
    QMetaObject::invokeMethod(color, "textEdited", Qt::DirectConnection,
                              Q_ARG(QString, QStringLiteral("#123456")));
    check(document.issues().isEmpty(), QStringLiteral("corrected color restores schema"));
    const int stepCount = document.data.value(QStringLiteral("steps")).toArray().size();
    QTimer::singleShot(0, this, [] {
        if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))
            box->button(QMessageBox::Yes)->click();
    });
    panel->findChild<QPushButton *>(QStringLiteral("deleteStep"))->click();
    check(document.data.value(QStringLiteral("steps")).toArray().size() == stepCount - 1,
          QStringLiteral("delete step confirmed through UI"));
    undo->undo();
    check(document.data.value(QStringLiteral("steps")).toArray().size() == stepCount,
          QStringLiteral("undo deleted step"));
    QTemporaryDir tmp;
    QString error;
    check(tmp.isValid() && document.saveDraft(tmp.filePath(QStringLiteral("test.d4workflow.json")), &error),
          QStringLiteral("edited UI document writes atomically"));
    const auto revision = document.data.value(QStringLiteral("revision"));
    undo->setClean();
    mutate(QStringLiteral("test name"), [this] { document.setName(QStringLiteral("Changed after save")); });
    undo->undo();
    check(!document.dirty && document.data.value(QStringLiteral("revision")) == revision,
          QStringLiteral("undo to clean preserves disk revision"));
    WorkflowDocument reopened;
    check(reopened.load(document.filePath, &error) && reopened.data == document.data,
          QStringLiteral("UI data round-trip matches JSON"));
    // Small overlay windows never take focus: binding changes must still replace the
    // previous object's text, and undo must update focused fields without losing IDs.
    showSteps();
    panel->showPoints(QStringLiteral("p1"));
    auto *pointName = panel->findChild<QLineEdit *>(QStringLiteral("pointName"));
    pointName->setFocus();
    qApp->processEvents();
    panel->showPoints(QStringLiteral("p2"));
    const auto pointsBefore = document.data.value(QStringLiteral("points")).toArray();
    check(pointName->text() == pointsBefore.at(1).toObject().value(QStringLiteral("name")).toString(),
          QStringLiteral("focused point field rebinds on overlay selection"));
    pointName->setText(QStringLiteral("Changed second point"));
    QMetaObject::invokeMethod(pointName, "textEdited", Qt::DirectConnection,
                              Q_ARG(QString, QStringLiteral("Changed second point")));
    check(document.data.value(QStringLiteral("points")).toArray().first() == pointsBefore.first(),
          QStringLiteral("point rename leaves previously bound point unchanged"));
    undo->undo();
    check(pointName->text() == pointsBefore.at(1).toObject().value(QStringLiteral("name")).toString(),
          QStringLiteral("undo updates focused bound fields"));
    const auto beforeView = document.data;
    referenceCrop = QRectF(.05, .05, .9, .9);
    overlay->setReferenceRect(referenceCrop);
    const auto stateFile = tmp.filePath(QStringLiteral("local-view.ini"));
    check(saveViewState(stateFile), QStringLiteral("local crop state persists without screenshot path"));
    referenceCrop = QRectF(0, 0, 1, 1);
    overlay->setReferenceRect(referenceCrop);
    loadViewState(stateFile);
    check(referenceCrop == QRectF(.05, .05, .9, .9) && document.data == beforeView,
          QStringLiteral("crop round-trip never changes shared workflow JSON"));
    return {{QStringLiteral("passed"), passed},
            {QStringLiteral("failed"), failed},
            {QStringLiteral("diagnostics"), diagnostics()}};
}
