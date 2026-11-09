#include "WorkflowPanel.h"

#include <QAbstractItemModel>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabWidget>
#include <QUuid>
#include <QVBoxLayout>

namespace {
QString str(const QJsonObject &o, const char *key) {
    return o.value(QLatin1String(key)).toString();
}
QJsonObject itemFor(const QJsonObject &doc, const char *key, const QString &id) {
    for (const auto &v : doc.value(QLatin1String(key)).toArray())
        if (str(v.toObject(), "id") == id)
            return v.toObject();
    return {};
}
void changeItem(WorkflowDocument *doc, const char *array, const QString &id, const QString &field,
                const QJsonValue &value) {
    auto items = doc->data.value(QLatin1String(array)).toArray();
    for (qsizetype i = 0; i < items.size(); ++i) {
        auto item = items[i].toObject();
        if (str(item, "id") != id)
            continue;
        item.insert(field, value);
        items[i] = item;
        doc->data.insert(QLatin1String(array), items);
        return;
    }
}
void fillList(QListWidget *list, const QJsonArray &array, bool numbered = false) {
    const QString selection =
        list->currentItem() ? list->currentItem()->data(Qt::UserRole).toString() : QString{};
    const int oldRow = list->currentRow();
    const QSignalBlocker b(list);
    list->clear();
    int selected = -1;
    for (int i = 0; i < array.size(); ++i) {
        const auto o = array[i].toObject();
        QString label = str(o, "name");
        if (numbered)
            label = QStringLiteral("%1   %2   · %3 ms")
                        .arg(i + 1)
                        .arg(label)
                        .arg(o.value(QStringLiteral("minDelayMs")).toInt());
        auto *item = new QListWidgetItem(label, list);
        item->setData(Qt::UserRole, str(o, "id"));
        if (str(o, "id") == selection)
            selected = i;
    }
    if (selected < 0 && list->count())
        selected = qBound(0, oldRow, list->count() - 1);
    list->setCurrentRow(selected);
}
void choose(QComboBox *box, const QVariant &value) {
    const QSignalBlocker b(box);
    box->setCurrentIndex(qMax(0, box->findData(value)));
}
void fillRefs(QComboBox *box, const QJsonArray &items, const QString &emptyLabel) {
    const QSignalBlocker b(box);
    box->clear();
    box->addItem(emptyLabel, QString{});
    for (const auto &v : items)
        box->addItem(str(v.toObject(), "name"), str(v.toObject(), "id"));
}
QWidget *page(QTabWidget *tabs, const QString &title, QVBoxLayout **layout) {
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *body = new QWidget;
    *layout = new QVBoxLayout(body);
    (*layout)->setContentsMargins(12, 12, 12, 12);
    scroll->setWidget(body);
    tabs->addTab(scroll, title);
    return body;
}
QPushButton *button(QHBoxLayout *row, const QString &text, const QString &id = {}) {
    auto *b = new QPushButton(text);
    b->setObjectName(id);
    b->setCursor(Qt::PointingHandCursor);
    row->addWidget(b);
    return b;
}
QSpinBox *milliseconds(int max = 600000) {
    auto *s = new QSpinBox;
    s->setRange(0, max);
    s->setSuffix(QStringLiteral(" ms"));
    s->setKeyboardTracking(false);
    return s;
}
QDoubleSpinBox *fraction() {
    auto *s = new QDoubleSpinBox;
    s->setRange(0, 1);
    s->setDecimals(5);
    s->setSingleStep(.001);
    s->setKeyboardTracking(false);
    return s;
}
} // namespace

WorkflowPanel::WorkflowPanel(WorkflowDocument *document, Mutation mutation, QWidget *parent)
    : QWidget(parent, Qt::Tool | Qt::WindowStaysOnTopHint), doc(document), mutate(std::move(mutation)) {
    setObjectName(QStringLiteral("stepsPanel"));
    setWindowTitle(QStringLiteral("D4Box · 流程编辑（不执行）"));
    setAttribute(Qt::WA_QuitOnClose, false);
    resize(438, 720);
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 8);
    tabs = new QTabWidget(this);
    root->addWidget(tabs);
    buildSteps();
    buildPoints();
    buildChecks();
    buildLayout();
    validation = new QLabel;
    validation->setObjectName(QStringLiteral("validation"));
    validation->setWordWrap(true);
    validation->setContentsMargins(12, 4, 12, 4);
    root->addWidget(validation);
    auto *close = new QPushButton(QStringLiteral("关闭面板 · 保留当前编辑"));
    root->addWidget(close);
    connect(close, &QPushButton::clicked, this, &QWidget::hide);
    refresh();
}

void WorkflowPanel::buildSteps() {
    QVBoxLayout *v;
    page(tabs, QStringLiteral("步骤"), &v);
    auto *form = new QFormLayout;
    sourceMode = new QComboBox;
    sourceMode->setObjectName(QStringLiteral("sourceMode"));
    sourceMode->addItem(QStringLiteral("单件 · 使用触发时鼠标所在物品"), QStringLiteral("hover_item"));
    sourceMode->addItem(QStringLiteral("循环 · 遍历背包一次（最多 33 件）"),
                        QStringLiteral("inventory_once"));
    sourceMode->addItem(QStringLiteral("手动 · 物品已预先放入"), QStringLiteral("preloaded"));
    form->addRow(QStringLiteral("目标来源"), sourceMode);
    maxItems = new QSpinBox;
    maxItems->setRange(1, 33);
    maxItems->setKeyboardTracking(false);
    form->addRow(QStringLiteral("最多处理"), maxItems);
    totalTimeout = milliseconds(1800000);
    totalTimeout->setMinimum(1000);
    form->addRow(QStringLiteral("总超时"), totalTimeout);
    auto *qrow = new QHBoxLayout;
    const QStringList labels{QStringLiteral("红"), QStringLiteral("黄"), QStringLiteral("蓝"),
                             QStringLiteral("暗"), QStringLiteral("未知")};
    const QStringList keys{QStringLiteral("red"), QStringLiteral("yellow"), QStringLiteral("blue"),
                           QStringLiteral("dark"), QStringLiteral("unknown")};
    for (int i = 0; i < labels.size(); ++i) {
        auto *c = new QCheckBox(labels[i]);
        c->setProperty("quality", keys[i]);
        qualities.append(c);
        qrow->addWidget(c);
        connect(c, &QCheckBox::toggled, this, [this] { editSource(); });
    }
    form->addRow(QStringLiteral("原始颜色筛选"), qrow);
    excluded = new QLineEdit;
    excluded->setObjectName(QStringLiteral("excludedSlots"));
    excluded->setPlaceholderText(QStringLiteral("跳过格位 1–33，例如 1, 2, 33"));
    form->addRow(QStringLiteral("排除格位"), excluded);
    v->addLayout(form);
    auto *hint = new QLabel(
        QStringLiteral("不勾颜色＝全部；颜色只是分类标签，不识别收藏/暗金。循环是有限队列，不是无限重复。"));
    hint->setWordWrap(true);
    v->addWidget(hint);
    auto *row = new QHBoxLayout;
    auto *add = button(row, QStringLiteral("＋ 新增步骤"), QStringLiteral("addStep"));
    auto *menu = new QMenu(add);
    for (const auto &pair :
         QList<QPair<QString, QString>>{{QStringLiteral("左键点击"), QStringLiteral("left")},
                                        {QStringLiteral("右键点击"), QStringLiteral("right")},
                                        {QStringLiteral("等待"), QStringLiteral("wait")},
                                        {QStringLiteral("状态检查"), QStringLiteral("check")}}) {
        auto *a = menu->addAction(pair.first);
        connect(a, &QAction::triggered, this, [this, value = pair.second] {
            addStep(value == QStringLiteral("left") || value == QStringLiteral("right")
                        ? QStringLiteral("click")
                        : value,
                    value);
        });
    }
    add->setMenu(menu);
    deleteStep = button(row, QStringLiteral("删除选中"), QStringLiteral("deleteStep"));
    deleteStep->setProperty("danger", true);
    v->addLayout(row);
    steps = new QListWidget;
    steps->setObjectName(QStringLiteral("stepsList"));
    steps->setMinimumHeight(135);
    steps->setMaximumHeight(240);
    steps->setDragDropMode(QAbstractItemView::InternalMove);
    steps->setDefaultDropAction(Qt::MoveAction);
    v->addWidget(steps);
    row = new QHBoxLayout;
    up = button(row, QStringLiteral("上移"), QStringLiteral("stepUp"));
    down = button(row, QStringLiteral("下移"), QStringLiteral("stepDown"));
    locate = button(row, QStringLiteral("定位点位"), QStringLiteral("locateStep"));
    v->addLayout(row);
    form = new QFormLayout;
    stepName = new QLineEdit;
    stepName->setObjectName(QStringLiteral("stepName"));
    form->addRow(QStringLiteral("步骤名"), stepName);
    action = new QComboBox;
    action->setObjectName(QStringLiteral("stepAction"));
    action->addItem(QStringLiteral("左键点击"), QStringLiteral("left"));
    action->addItem(QStringLiteral("右键点击"), QStringLiteral("right"));
    action->addItem(QStringLiteral("等待"), QStringLiteral("wait"));
    action->addItem(QStringLiteral("状态检查"), QStringLiteral("check"));
    form->addRow(QStringLiteral("动作"), action);
    target = new QComboBox;
    form->addRow(QStringLiteral("目标"), target);
    risk = new QComboBox;
    risk->addItem(QStringLiteral("消耗 / 修改物品（需状态转换检查）"), QStringLiteral("mutating"));
    risk->addItem(QStringLiteral("只切换 / 选择界面（需状态存在检查）"), QStringLiteral("none"));
    form->addRow(QStringLiteral("点击风险"), risk);
    quality = new QComboBox;
    quality->addItem(QStringLiteral("任意原始颜色"), QString{});
    for (int i = 0; i < keys.size(); ++i)
        quality->addItem(labels[i], keys[i]);
    form->addRow(QStringLiteral("仅当原始颜色"), quality);
    delay = milliseconds(10000);
    form->addRow(QStringLiteral("最短间隔 / 等待"), delay);
    timeout = milliseconds(30000);
    timeout->setMinimum(100);
    form->addRow(QStringLiteral("步骤超时"), timeout);
    completion = new QComboBox;
    form->addRow(QStringLiteral("完成条件"), completion);
    stepCheck = new QComboBox;
    form->addRow(QStringLiteral("检查规则"), stepCheck);
    v->addLayout(form);
    v->addStretch();
    connect(sourceMode, &QComboBox::activated, this, [this] { editSource(); });
    connect(maxItems, &QSpinBox::valueChanged, this, [this] { editSource(); });
    connect(totalTimeout, &QSpinBox::valueChanged, this, [this] { editSource(); });
    connect(excluded, &QLineEdit::textEdited, this, [this] { editSource(); });
    connect(steps, &QListWidget::currentRowChanged, this, [this] {
        if (!refreshing)
            loadStep();
    });
    connect(stepName, &QLineEdit::textEdited, this,
            [this](const QString &t) { editStep(QStringLiteral("name"), t); });
    connect(action, &QComboBox::activated, this, [this] {
        const QString id = selectedId(steps), a = action->currentData().toString();
        if (id.isEmpty())
            return;
        mutate(QStringLiteral("修改动作"), [this, id, a] {
            const bool click = a == QStringLiteral("left") || a == QStringLiteral("right");
            auto items = doc->data.value(QStringLiteral("steps")).toArray();
            for (qsizetype i = 0; i < items.size(); ++i) {
                auto item = items[i].toObject();
                if (str(item, "id") != id)
                    continue;
                const auto defaults = WorkflowDocument::makeStep(click ? QStringLiteral("click") : a);
                for (auto it = defaults.begin(); it != defaults.end(); ++it)
                    if (!item.contains(it.key()))
                        item.insert(it.key(), it.value());
                item.insert(QStringLiteral("action"), click ? QStringLiteral("click") : a);
                if (click) {
                    item.insert(QStringLiteral("button"), a);
                    if (!item.value(QStringLiteral("target")).isObject())
                        item.insert(QStringLiteral("target"),
                                    QJsonObject{{QStringLiteral("type"), QStringLiteral("current_item")}});
                } else
                    for (const auto &key : {QStringLiteral("button"), QStringLiteral("target"),
                                            QStringLiteral("risk"), QStringLiteral("completionCheckId")})
                        item.remove(key);
                if (a != QStringLiteral("check"))
                    item.remove(QStringLiteral("checkId"));
                items[i] = item;
                break;
            }
            doc->data.insert(QStringLiteral("steps"), items);
        });
    });
    connect(target, &QComboBox::activated, this, [this] {
        const QString id = target->currentData().toString();
        editStep(QStringLiteral("target"),
                 id.isEmpty() ? QJsonObject{{QStringLiteral("type"), QStringLiteral("current_item")}}
                              : QJsonObject{{QStringLiteral("type"), QStringLiteral("point")},
                                            {QStringLiteral("pointId"), id}});
    });
    connect(risk, &QComboBox::activated, this,
            [this] { editStep(QStringLiteral("risk"), risk->currentData().toString()); });
    connect(quality, &QComboBox::activated, this, [this] {
        QJsonArray q;
        if (!quality->currentData().toString().isEmpty())
            q.append(quality->currentData().toString());
        editStep(QStringLiteral("originalQualities"), q);
    });
    connect(delay, &QSpinBox::valueChanged, this, [this](int n) {
        if (!refreshing)
            editStep(QStringLiteral("minDelayMs"), n);
    });
    connect(timeout, &QSpinBox::valueChanged, this, [this](int n) {
        if (!refreshing)
            editStep(QStringLiteral("timeoutMs"), n);
    });
    connect(completion, &QComboBox::activated, this, [this] {
        const auto id = completion->currentData().toString();
        editStep(QStringLiteral("completionCheckId"), id.isEmpty() ? QJsonValue::Null : QJsonValue(id));
    });
    connect(stepCheck, &QComboBox::activated, this, [this] {
        const auto id = stepCheck->currentData().toString();
        editStep(QStringLiteral("checkId"), id.isEmpty() ? QJsonValue::Null : QJsonValue(id));
    });
    connect(deleteStep, &QPushButton::clicked, this, [this] {
        const int i = steps->currentRow();
        if (i < 0)
            return;
        if (QMessageBox::question(this, QStringLiteral("删除步骤"),
                                  QStringLiteral("删除第 %1 步“%2”？可通过撤销恢复；不删除点位。")
                                      .arg(i + 1)
                                      .arg(stepName->text()),
                                  QMessageBox::Yes | QMessageBox::Cancel,
                                  QMessageBox::Cancel) != QMessageBox::Yes)
            return;
        mutate(QStringLiteral("删除步骤"), [this, i] {
            auto s = doc->data.value(QStringLiteral("steps")).toArray();
            s.removeAt(i);
            doc->data.insert(QStringLiteral("steps"), s);
        });
    });
    auto move = [this](int delta) {
        const int from = steps->currentRow(), to = from + delta;
        if (from < 0 || to < 0 || to >= steps->count())
            return;
        mutate(QStringLiteral("调整步骤顺序"), [this, from, to] {
            auto s = doc->data.value(QStringLiteral("steps")).toArray();
            auto value = s.takeAt(from);
            s.insert(to, value);
            doc->data.insert(QStringLiteral("steps"), s);
        });
    };
    connect(up, &QPushButton::clicked, this, [move] { move(-1); });
    connect(down, &QPushButton::clicked, this, [move] { move(1); });
    connect(steps->model(), &QAbstractItemModel::rowsMoved, this, [this] {
        if (refreshing)
            return;
        const auto documentId = doc->data.value(QStringLiteral("id"));
        QJsonArray sorted;
        for (int i = 0; i < steps->count(); ++i)
            sorted.append(itemFor(doc->data, "steps", steps->item(i)->data(Qt::UserRole).toString()));
        // Let the item-view finish its internal move before rebuilding its model.
        QMetaObject::invokeMethod(
            this,
            [this, sorted, documentId] {
                if (doc->data.value(QStringLiteral("id")) != documentId)
                    return;
                mutate(QStringLiteral("拖动步骤排序"),
                       [this, sorted] { doc->data.insert(QStringLiteral("steps"), sorted); });
            },
            Qt::QueuedConnection);
    });
    connect(locate, &QPushButton::clicked, this, [this] {
        if (locatePoint && !target->currentData().toString().isEmpty())
            locatePoint(target->currentData().toString());
    });
}

void WorkflowPanel::buildPoints() {
    QVBoxLayout *v;
    page(tabs, QStringLiteral("点位"), &v);
    auto *row = new QHBoxLayout;
    auto *add = button(row, QStringLiteral("＋ 新增点位"), QStringLiteral("addPoint"));
    deletePoint = button(row, QStringLiteral("删除点位"));
    deletePoint->setProperty("danger", true);
    v->addLayout(row);
    points = new QListWidget;
    points->setObjectName(QStringLiteral("pointsList"));
    points->setMinimumHeight(140);
    v->addWidget(points);
    auto *form = new QFormLayout;
    pointName = new QLineEdit;
    pointName->setObjectName(QStringLiteral("pointName"));
    form->addRow(QStringLiteral("名称"), pointName);
    pointX = fraction();
    pointY = fraction();
    form->addRow(QStringLiteral("X（0–1）"), pointX);
    form->addRow(QStringLiteral("Y（0–1）"), pointY);
    v->addLayout(form);
    pointState = new QLabel;
    pointState->setWordWrap(true);
    v->addWidget(pointState);
    row = new QHBoxLayout;
    placePoint = button(row, QStringLiteral("放到参考区中心"));
    auto *locateButton = button(row, QStringLiteral("定位 / 避让面板"));
    v->addLayout(row);
    auto *hint = new QLabel(QStringLiteral("拖动图标定位，双击图标改名。空白和背包内部不拦截鼠标。删除被步骤"
                                           "或检查引用的点位前，请先解绑引用。"));
    hint->setWordWrap(true);
    v->addWidget(hint);
    v->addStretch();
    connect(add, &QPushButton::clicked, this, [this] {
        QString id;
        mutate(QStringLiteral("新增点位"), [this, &id] {
            id = doc->addPoint(QStringLiteral("新点位 %1")
                                   .arg(doc->data.value(QStringLiteral("points")).toArray().size() + 1),
                               QPointF(.5, .5));
        });
        showPoints(id);
    });
    connect(points, &QListWidget::currentRowChanged, this, [this] {
        if (!refreshing)
            loadPoint();
    });
    connect(pointName, &QLineEdit::textEdited, this, [this](const QString &value) {
        const QString id = selectedId(points);
        mutate(QStringLiteral("重命名点位"), [this, id, value] { doc->renamePoint(id, value); });
    });
    auto reposition = [this] {
        if (refreshing)
            return;
        const auto id = selectedId(points);
        const QPointF p(pointX->value(), pointY->value());
        mutate(QStringLiteral("调整点位坐标"), [this, id, p] { doc->setPointPosition(id, p); });
    };
    connect(pointX, &QDoubleSpinBox::valueChanged, this, reposition);
    connect(pointY, &QDoubleSpinBox::valueChanged, this, reposition);
    connect(placePoint, &QPushButton::clicked, this, [this] {
        const QString id = selectedId(points);
        mutate(QStringLiteral("放置点位"), [this, id] { doc->setPointPosition(id, QPointF(.5, .5)); });
        if (locatePoint)
            locatePoint(id);
    });
    connect(locateButton, &QPushButton::clicked, this, [this] {
        if (locatePoint)
            locatePoint(selectedId(points));
    });
    connect(deletePoint, &QPushButton::clicked, this, [this] {
        const auto id = selectedId(points);
        QString error;
        mutate(QStringLiteral("删除点位"), [this, id, &error] { doc->removePoint(id, &error); });
        if (!error.isEmpty())
            QMessageBox::information(this, QStringLiteral("点位仍被使用"), error);
    });
}

void WorkflowPanel::buildChecks() {
    QVBoxLayout *v;
    page(tabs, QStringLiteral("检查"), &v);
    auto *row = new QHBoxLayout;
    auto *add = button(row, QStringLiteral("＋ 新增颜色检查"), QStringLiteral("addCheck"));
    deleteCheck = button(row, QStringLiteral("删除检查"));
    deleteCheck->setProperty("danger", true);
    v->addLayout(row);
    checks = new QListWidget;
    checks->setMinimumHeight(120);
    v->addWidget(checks);
    auto *form = new QFormLayout;
    checkName = new QLineEdit;
    form->addRow(QStringLiteral("名称"), checkName);
    checkPoint = new QComboBox;
    form->addRow(QStringLiteral("采样点"), checkPoint);
    color = new QLineEdit;
    color->setObjectName(QStringLiteral("checkColor"));
    color->setPlaceholderText(QStringLiteral("#RRGGBB；留空表示待标定"));
    form->addRow(QStringLiteral("目标颜色"), color);
    tolerance = new QSpinBox;
    tolerance->setRange(0, 255);
    tolerance->setKeyboardTracking(false);
    form->addRow(QStringLiteral("RGB 容差"), tolerance);
    samples = new QSpinBox;
    samples->setRange(1, 20);
    samples->setKeyboardTracking(false);
    form->addRow(QStringLiteral("连续稳定样本"), samples);
    checkMode = new QComboBox;
    checkMode->addItem(QStringLiteral("当前状态存在"), QStringLiteral("present"));
    checkMode->addItem(QStringLiteral("动作后由不匹配变为匹配"), QStringLiteral("transition"));
    form->addRow(QStringLiteral("判定方式"), checkMode);
    entryCheck = new QComboBox;
    form->addRow(QStringLiteral("流程开始前检查"), entryCheck);
    v->addLayout(form);
    auto *hint = new QLabel(QStringLiteral("这里只配置规则，不取色、不截图、不识别游戏。请使用真实样本填写颜"
                                           "色；未配置的规则可存草稿，但静态就绪检查不会通过。"));
    hint->setWordWrap(true);
    v->addWidget(hint);
    v->addStretch();
    connect(add, &QPushButton::clicked, this, [this] {
        const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        mutate(QStringLiteral("新增检查"), [this, id] {
            auto a = doc->data.value(QStringLiteral("checks")).toArray();
            a.append(QJsonObject{{QStringLiteral("id"), id},
                                 {QStringLiteral("name"), QStringLiteral("新颜色检查")},
                                 {QStringLiteral("type"), QStringLiteral("pixel")},
                                 {QStringLiteral("pointId"), QJsonValue::Null},
                                 {QStringLiteral("expectedColor"), QJsonValue::Null},
                                 {QStringLiteral("tolerance"), 24},
                                 {QStringLiteral("stableSamples"), 3},
                                 {QStringLiteral("mode"), QStringLiteral("present")}});
            doc->data.insert(QStringLiteral("checks"), a);
        });
        checks->setCurrentRow(checks->count() - 1);
    });
    connect(checks, &QListWidget::currentRowChanged, this, [this] {
        if (!refreshing)
            loadCheck();
    });
    connect(checkName, &QLineEdit::textEdited, this,
            [this](const QString &t) { editCheck(QStringLiteral("name"), t); });
    connect(checkPoint, &QComboBox::activated, this, [this] {
        const auto id = checkPoint->currentData().toString();
        editCheck(QStringLiteral("pointId"), id.isEmpty() ? QJsonValue::Null : QJsonValue(id));
    });
    connect(color, &QLineEdit::textEdited, this, [this](const QString &text) {
        editCheck(QStringLiteral("expectedColor"),
                  text.trimmed().isEmpty() ? QJsonValue::Null : QJsonValue(text.trimmed()));
    });
    connect(color, &QLineEdit::editingFinished, this, [this] {
        const auto text = color->text().trimmed();
        if (!text.isEmpty() &&
            !QRegularExpression(QStringLiteral("^#[0-9a-fA-F]{6}$")).match(text).hasMatch()) {
            color->setToolTip(QStringLiteral("格式为 #RRGGBB；输入仍保留，保存前请修正。"));
            color->setStyleSheet(QStringLiteral("border:1px solid #C98282"));
            return;
        }
        color->setStyleSheet({});
        color->setToolTip({});
        editCheck(QStringLiteral("expectedColor"),
                  text.isEmpty() ? QJsonValue::Null : QJsonValue(text.toUpper()));
    });
    connect(tolerance, &QSpinBox::valueChanged, this, [this](int n) {
        if (!refreshing)
            editCheck(QStringLiteral("tolerance"), n);
    });
    connect(samples, &QSpinBox::valueChanged, this, [this](int n) {
        if (!refreshing)
            editCheck(QStringLiteral("stableSamples"), n);
    });
    connect(checkMode, &QComboBox::activated, this,
            [this] { editCheck(QStringLiteral("mode"), checkMode->currentData().toString()); });
    connect(entryCheck, &QComboBox::activated, this, [this] {
        const auto id = entryCheck->currentData().toString();
        mutate(QStringLiteral("入口检查"), [this, id] {
            doc->data.insert(QStringLiteral("entryCheckId"),
                             id.isEmpty() ? QJsonValue::Null : QJsonValue(id));
        });
    });
    connect(deleteCheck, &QPushButton::clicked, this, [this] {
        const auto id = selectedId(checks);
        if (id.isEmpty())
            return;
        bool used = doc->data.value(QStringLiteral("entryCheckId")) == id;
        for (const auto &s : doc->data.value(QStringLiteral("steps")).toArray())
            used |= s.toObject().value(QStringLiteral("checkId")) == id ||
                    s.toObject().value(QStringLiteral("completionCheckId")) == id;
        if (used) {
            QMessageBox::information(this, QStringLiteral("检查仍被引用"),
                                     QStringLiteral("请先移除步骤及入口的检查引用，再删除规则。"));
            return;
        }
        mutate(QStringLiteral("删除检查"), [this, id] {
            auto a = doc->data.value(QStringLiteral("checks")).toArray();
            for (qsizetype i = 0; i < a.size(); ++i)
                if (str(a[i].toObject(), "id") == id) {
                    a.removeAt(i);
                    break;
                }
            doc->data.insert(QStringLiteral("checks"), a);
        });
    });
}

void WorkflowPanel::buildLayout() {
    QVBoxLayout *v;
    page(tabs, QStringLiteral("布局"), &v);
    auto *form = new QFormLayout;
    description = new QLineEdit;
    form->addRow(QStringLiteral("配置说明"), description);
    layouts = new QComboBox;
    form->addRow(QStringLiteral("参考布局"), layouts);
    v->addLayout(form);
    connect(description, &QLineEdit::textEdited, this, [this](const QString &value) {
        mutate(QStringLiteral("修改说明"),
               [this, value] { doc->data.insert(QStringLiteral("description"), value); });
    });
    connect(layouts, &QComboBox::activated, this, [this] {
        const auto id = layouts->currentData().toString();
        mutate(QStringLiteral("选择布局"),
               [this, id] { doc->data.insert(QStringLiteral("defaultLayoutId"), id); });
    });
    auto *row = new QHBoxLayout;
    auto *copy = button(row, QStringLiteral("复制布局"));
    auto *remove = button(row, QStringLiteral("删除布局"));
    v->addLayout(row);
    connect(copy, &QPushButton::clicked, this, [this] {
        mutate(QStringLiteral("复制布局"), [this] {
            auto layout = doc->layout();
            const auto id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            layout.insert(QStringLiteral("id"), id);
            layout.insert(QStringLiteral("name"), str(layout, "name") + QStringLiteral(" · 副本"));
            layout.insert(QStringLiteral("calibration"), QStringLiteral("unverified"));
            auto a = doc->data.value(QStringLiteral("layouts")).toArray();
            a.append(layout);
            doc->data.insert(QStringLiteral("layouts"), a);
            doc->data.insert(QStringLiteral("defaultLayoutId"), id);
        });
    });
    connect(remove, &QPushButton::clicked, this, [this] {
        auto a = doc->data.value(QStringLiteral("layouts")).toArray();
        if (a.size() < 2)
            return;
        const auto id = str(doc->data, "defaultLayoutId");
        mutate(QStringLiteral("删除布局"), [this, a, id]() mutable {
            for (qsizetype i = 0; i < a.size(); ++i)
                if (str(a[i].toObject(), "id") == id) {
                    a.removeAt(i);
                    break;
                }
            doc->data.insert(QStringLiteral("layouts"), a);
            doc->data.insert(QStringLiteral("defaultLayoutId"),
                             a.first().toObject().value(QStringLiteral("id")));
        });
    });
    v->addWidget(new QLabel(QStringLiteral("背包 · 3 行 × 11 列（归一化）")));
    auto *placeInventory = new QPushButton(QStringLiteral("放置背包框（初始位置，需手动调整）"));
    v->addWidget(placeInventory);
    connect(placeInventory, &QPushButton::clicked, this, [this] {
        mutate(QStringLiteral("放置背包框"), [this] { doc->setInventoryRect(QRectF(.66, .67, .315, .225)); });
    });
    form = new QFormLayout;
    const QStringList titles{QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("宽"),
                             QStringLiteral("高")};
    for (int i = 0; i < 4; ++i) {
        inventory[i] = fraction();
        form->addRow(titles[i], inventory[i]);
        connect(inventory[i], &QDoubleSpinBox::valueChanged, this, [this] {
            if (refreshing)
                return;
            const QRectF r(inventory[0]->value(), inventory[1]->value(), inventory[2]->value(),
                           inventory[3]->value());
            mutate(QStringLiteral("调整背包"), [this, r] { doc->setInventoryRect(r); });
        });
    }
    v->addLayout(form);
    v->addWidget(new QLabel(QStringLiteral("有效参考区域 · 相对整屏 / 整张底图")));
    form = new QFormLayout;
    for (int i = 0; i < 4; ++i) {
        crop[i] = fraction();
        form->addRow(titles[i], crop[i]);
    }
    crop[2]->setValue(1);
    crop[3]->setValue(1);
    v->addLayout(form);
    auto *apply = new QPushButton(QStringLiteral("应用参考区（不保存个人屏幕位置）"));
    v->addWidget(apply);
    connect(apply, &QPushButton::clicked, this, [this] {
        const QRectF r(crop[0]->value(), crop[1]->value(), crop[2]->value(), crop[3]->value());
        if (r.width() < .05 || r.height() < .05 || r.right() > 1.00001 || r.bottom() > 1.00001) {
            QMessageBox::warning(this, QStringLiteral("参考区不合法"),
                                 QStringLiteral("宽高至少 0.05，且不得超出 0–1 范围。"));
            return;
        }
        if (cropChanged)
            cropChanged(r);
    });
    auto *confirm = new QPushButton(QStringLiteral("确认当前参考尺寸 / 人工校准"));
    v->addWidget(confirm);
    connect(confirm, &QPushButton::clicked, this, [this] {
        if (confirmReference)
            confirmReference();
    });
    auto *hint = new QLabel(QStringLiteral(
        "移动图标、背包或更换参考区后会回到待校准。人工校准只是记录坐标参考，不能证明游戏执行正确。"));
    hint->setWordWrap(true);
    v->addWidget(hint);
    v->addStretch();
}

QString WorkflowPanel::selectedId(QListWidget *list) const {
    return list->currentItem() ? list->currentItem()->data(Qt::UserRole).toString() : QString{};
}

void WorkflowPanel::editStep(const QString &field, const QJsonValue &value) {
    if (refreshing)
        return;
    const auto id = selectedId(steps);
    if (id.isEmpty())
        return;
    mutate(QStringLiteral("修改步骤"),
           [this, id, field, value] { changeItem(doc, "steps", id, field, value); });
}
void WorkflowPanel::editCheck(const QString &field, const QJsonValue &value) {
    if (refreshing)
        return;
    const auto id = selectedId(checks);
    if (id.isEmpty())
        return;
    mutate(QStringLiteral("修改检查"),
           [this, id, field, value] { changeItem(doc, "checks", id, field, value); });
}
void WorkflowPanel::addStep(const QString &type, const QString &mouseButton) {
    const int position = steps->currentRow() < 0 ? steps->count() : steps->currentRow() + 1;
    mutate(QStringLiteral("新增步骤"), [this, type, mouseButton, position] {
        auto step = WorkflowDocument::makeStep(type);
        if (type == QStringLiteral("click")) {
            step.insert(QStringLiteral("button"), mouseButton);
            step.insert(QStringLiteral("target"),
                        QJsonObject{{QStringLiteral("type"), QStringLiteral("current_item")}});
            step.insert(QStringLiteral("name"), mouseButton == QStringLiteral("right")
                                                    ? QStringLiteral("右键点击")
                                                    : QStringLiteral("左键点击"));
        }
        auto a = doc->data.value(QStringLiteral("steps")).toArray();
        a.insert(position, step);
        doc->data.insert(QStringLiteral("steps"), a);
    });
    steps->setCurrentRow(position);
}
void WorkflowPanel::editSource() {
    if (refreshing)
        return;
    QJsonArray skips;
    bool validSlots = true;
    for (const auto &part :
         excluded->text().split(QRegularExpression(QStringLiteral("[,，\\s]+")), Qt::SkipEmptyParts)) {
        bool ok;
        const int n = part.toInt(&ok);
        if (!ok || n < 1 || n > 33) {
            validSlots = false;
            break;
        }
        if (!skips.contains(n - 1))
            skips.append(n - 1);
    }
    excluded->setStyleSheet(validSlots ? QString{} : QStringLiteral("border:1px solid #C98282"));
    excluded->setToolTip(validSlots ? QString{}
                                    : QStringLiteral("只接受 1–33 格位，以逗号分隔；保存前请修正。"));
    const QJsonValue slotValue = validSlots ? QJsonValue(skips) : QJsonValue(excluded->text());
    QJsonArray colors;
    for (auto *c : qualities)
        if (c->isChecked())
            colors.append(c->property("quality").toString());
    const auto mode = sourceMode->currentData().toString();
    const int n = mode == QStringLiteral("inventory_once") ? maxItems->value() : 1,
              ms = totalTimeout->value();
    mutate(QStringLiteral("修改目标来源"), [this, mode, n, ms, colors, slotValue] {
        auto s = doc->data.value(QStringLiteral("source")).toObject();
        s.insert(QStringLiteral("mode"), mode);
        s.insert(QStringLiteral("maxItems"), n);
        s.insert(QStringLiteral("originalQualities"), colors);
        s.insert(QStringLiteral("excludedSlots"), slotValue);
        doc->data.insert(QStringLiteral("source"), s);
        auto limits = doc->data.value(QStringLiteral("limits")).toObject();
        limits.insert(QStringLiteral("maxItemsPerRun"), n);
        limits.insert(QStringLiteral("totalTimeoutMs"), ms);
        doc->data.insert(QStringLiteral("limits"), limits);
    });
}

void WorkflowPanel::refresh() {
    refreshing = true;
    const auto source = doc->data.value(QStringLiteral("source")).toObject();
    choose(sourceMode, str(source, "mode"));
    maxItems->setValue(source.value(QStringLiteral("maxItems")).toInt(1));
    maxItems->setEnabled(sourceMode->currentData() == QStringLiteral("inventory_once"));
    totalTimeout->setValue(doc->data.value(QStringLiteral("limits"))
                               .toObject()
                               .value(QStringLiteral("totalTimeoutMs"))
                               .toInt(60000));
    for (auto *c : qualities)
        c->setChecked(source.value(QStringLiteral("originalQualities"))
                          .toArray()
                          .contains(c->property("quality").toString()));
    if (!excluded->hasFocus()) {
        QStringList s;
        const auto slotData = source.value(QStringLiteral("excludedSlots"));
        for (auto i : slotData.toArray())
            s.append(QString::number(i.toInt() + 1));
        excluded->setText(slotData.isString() ? slotData.toString() : s.join(QStringLiteral(", ")));
        excluded->setStyleSheet(slotData.isString() ? QStringLiteral("border:1px solid #C98282") : QString{});
    }
    fillList(steps, doc->data.value(QStringLiteral("steps")).toArray(), true);
    fillList(points, doc->data.value(QStringLiteral("points")).toArray());
    fillList(checks, doc->data.value(QStringLiteral("checks")).toArray());
    fillRefs(completion, doc->data.value(QStringLiteral("checks")).toArray(),
             QStringLiteral("未设置 · 只存草稿"));
    fillRefs(stepCheck, doc->data.value(QStringLiteral("checks")).toArray(), QStringLiteral("待选择规则"));
    fillRefs(entryCheck, doc->data.value(QStringLiteral("checks")).toArray(),
             QStringLiteral("待设置入口检查"));
    choose(entryCheck, doc->data.value(QStringLiteral("entryCheckId")).toString());
    fillRefs(target, doc->data.value(QStringLiteral("points")).toArray(),
             QStringLiteral("当前物品 · 动态位置"));
    fillRefs(checkPoint, doc->data.value(QStringLiteral("points")).toArray(), QStringLiteral("待选择采样点"));
    {
        const QSignalBlocker b(layouts);
        layouts->clear();
        for (const auto &l : doc->data.value(QStringLiteral("layouts")).toArray())
            layouts->addItem(str(l.toObject(), "name"), str(l.toObject(), "id"));
        choose(layouts, str(doc->data, "defaultLayoutId"));
    }
    const auto rect =
        doc->layout().value(QStringLiteral("inventory")).toObject().value(QStringLiteral("rect")).toObject();
    const QStringList keys{QStringLiteral("x"), QStringLiteral("y"), QStringLiteral("width"),
                           QStringLiteral("height")};
    for (int i = 0; i < 4; ++i)
        inventory[i]->setValue(rect.value(keys[i]).toDouble());
    if (description->text() != str(doc->data, "description"))
        description->setText(str(doc->data, "description"));
    refreshing = false;
    loadStep();
    loadPoint();
    loadCheck();
    auto issues = doc->issues(true);
    validation->setText(issues.isEmpty()
                            ? QStringLiteral("静态前提完整 · 仍只保存草稿；未验证游戏")
                            : QStringLiteral("草稿待完善 %1 项 · %2").arg(issues.size()).arg(issues.first()));
    validation->setToolTip(issues.join(QLatin1Char('\n')));
}
void WorkflowPanel::loadStep() {
    refreshing = true;
    const auto step = itemFor(doc->data, "steps", selectedId(steps));
    const bool valid = !step.isEmpty();
    for (auto *w : QList<QWidget *>{stepName, action, target, quality, risk, delay, timeout, completion,
                                    stepCheck, deleteStep})
        w->setEnabled(valid);
    if (stepName->text() != str(step, "name"))
        stepName->setText(str(step, "name"));
    const auto type = str(step, "action");
    choose(action, type == QStringLiteral("click") ? str(step, "button") : type);
    const auto t = step.value(QStringLiteral("target")).toObject();
    choose(target, str(t, "type") == QStringLiteral("point") ? str(t, "pointId") : QString{});
    auto qs = step.value(QStringLiteral("originalQualities")).toArray();
    choose(quality, qs.isEmpty() ? QString{} : qs.first().toString());
    delay->setValue(step.value(QStringLiteral("minDelayMs")).toInt());
    timeout->setValue(step.value(QStringLiteral("timeoutMs")).toInt(3000));
    choose(completion, str(step, "completionCheckId"));
    choose(stepCheck, str(step, "checkId"));
    choose(risk, str(step, "risk"));
    risk->setEnabled(valid && type == QStringLiteral("click"));
    target->setEnabled(valid && type == QStringLiteral("click"));
    completion->setEnabled(valid && type == QStringLiteral("click"));
    stepCheck->setEnabled(valid && type == QStringLiteral("check"));
    up->setEnabled(steps->currentRow() > 0);
    down->setEnabled(valid && steps->currentRow() < steps->count() - 1);
    locate->setEnabled(valid && type == QStringLiteral("click") && !str(t, "pointId").isEmpty());
    refreshing = false;
}
void WorkflowPanel::loadPoint() {
    refreshing = true;
    const auto id = selectedId(points);
    const auto p = itemFor(doc->data, "points", id);
    const bool valid = !p.isEmpty();
    const auto pos = doc->layout().value(QStringLiteral("pointPositions")).toObject().value(id);
    const bool placed = pos.isObject();
    for (auto *w : QList<QWidget *>{pointName, deletePoint, placePoint})
        w->setEnabled(valid);
    pointX->setEnabled(valid && placed);
    pointY->setEnabled(valid && placed);
    if (pointName->text() != str(p, "name"))
        pointName->setText(str(p, "name"));
    pointX->setValue(pos.toObject().value(QStringLiteral("x")).toDouble());
    pointY->setValue(pos.toObject().value(QStringLiteral("y")).toDouble());
    pointState->setText(!valid   ? QStringLiteral("请选择点位")
                        : placed ? QStringLiteral("已放置 · 可在透明层拖动")
                                 : QStringLiteral("未放置 · 不会编造坐标；点击“放到参考区中心”开始校准"));
    refreshing = false;
}
void WorkflowPanel::loadCheck() {
    refreshing = true;
    const auto c = itemFor(doc->data, "checks", selectedId(checks));
    const bool valid = !c.isEmpty();
    for (auto *w : QList<QWidget *>{checkName, checkPoint, color, tolerance, samples, checkMode, deleteCheck})
        w->setEnabled(valid);
    if (checkName->text() != str(c, "name"))
        checkName->setText(str(c, "name"));
    choose(checkPoint, str(c, "pointId"));
    if (color->text() != str(c, "expectedColor")) {
        color->setText(str(c, "expectedColor"));
        const auto value = str(c, "expectedColor");
        color->setStyleSheet(
            !value.isEmpty() &&
                    !QRegularExpression(QStringLiteral("^#[0-9a-fA-F]{6}$")).match(value).hasMatch()
                ? QStringLiteral("border:1px solid #C98282")
                : QString{});
    }
    tolerance->setValue(c.value(QStringLiteral("tolerance")).toInt(24));
    samples->setValue(c.value(QStringLiteral("stableSamples")).toInt(3));
    choose(checkMode, str(c, "mode"));
    refreshing = false;
}
void WorkflowPanel::showPoints(const QString &id) {
    tabs->setCurrentIndex(1);
    if (!id.isEmpty())
        for (int i = 0; i < points->count(); ++i)
            if (points->item(i)->data(Qt::UserRole).toString() == id)
                points->setCurrentRow(i);
}
void WorkflowPanel::showLayout() {
    tabs->setCurrentIndex(3);
}

void WorkflowPanel::setReferenceCrop(const QRectF &rect) {
    crop[0]->setValue(rect.x());
    crop[1]->setValue(rect.y());
    crop[2]->setValue(rect.width());
    crop[3]->setValue(rect.height());
}
