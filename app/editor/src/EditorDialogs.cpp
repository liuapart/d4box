#include "EditorDialogs.h"
#include "WindowChrome.h"
#include <QAbstractButton>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QGuiApplication>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScreen>
#include <QScrollArea>
#include <QSizeGrip>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>
#include <QWindow>

EditorDialog::EditorDialog(QWidget *parent, const QString &title, QScreen *screen)
    : QDialog(parent, Qt::Tool), targetScreen(screen ? screen : parent ? parent->screen() : nullptr) {
    setProperty("d4boxFrameless", true);
    setWindowTitle(title);
    setWindowModality(Qt::ApplicationModal);
    body = (new WindowChrome(this, title))->contentLayout();
}
void EditorDialog::setVisible(bool visible) {
    auto center = [this] {
        auto *display = targetScreen ? targetScreen.data() : QGuiApplication::primaryScreen();
        if (!display) return;
        if (windowHandle()) windowHandle()->setScreen(display);
        const auto area = display->availableGeometry();
        move(area.x() + (area.width() - width()) / 2, area.y() + (area.height() - height()) / 2);
    };
    if (visible) center();
    QDialog::setVisible(visible);
    if (visible) center(); // Reapply after the native host settles its geometry.
}

namespace {
QPixmap messageIcon(EditorMessageBox::Icon kind, qreal dpr) {
    QPixmap result(qRound(40 * dpr), qRound(40 * dpr));
    result.setDevicePixelRatio(dpr);
    result.fill(Qt::transparent);
    QPainter p(&result);
    p.setRenderHint(QPainter::Antialiasing);
    const bool warning = kind == EditorMessageBox::Warning || kind == EditorMessageBox::Critical;
    const QColor accent(warning ? QStringLiteral("#F1C27A") : QStringLiteral("#71F4DE"));
    p.setPen(QPen(accent, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(QColor(QStringLiteral("#20323F")));
    p.drawEllipse(QRectF(3, 3, 34, 34));
    p.setBrush(Qt::NoBrush);
    if (kind == EditorMessageBox::Question) {
        QPainterPath question;
        question.moveTo(14, 14);
        question.cubicTo(15, 8, 27, 8, 27, 16);
        question.cubicTo(27, 21, 20, 21, 20, 25);
        p.drawPath(question);
        p.drawPoint(QPointF(20, 30));
    } else if (warning) {
        p.drawLine(QPointF(20, 11), QPointF(20, 24));
        p.drawPoint(QPointF(20, 29));
    } else {
        p.drawPoint(QPointF(20, 12));
        p.drawLine(QPointF(20, 18), QPointF(20, 29));
    }
    return result;
}
}

EditorMessageBox::EditorMessageBox(QWidget *parent) : EditorDialog(parent, QStringLiteral("提示")) {
    setObjectName(QStringLiteral("editorMessageDialog"));
    setMinimumWidth(360);
    auto *row = new QHBoxLayout;
    symbol = new QLabel;
    symbol->setObjectName(QStringLiteral("dialogSymbol"));
    symbol->setFixedSize(44, 44);
    row->addWidget(symbol, 0, Qt::AlignTop);
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setMinimumHeight(52);
    auto *content = new QWidget;
    auto *textLayout = new QVBoxLayout(content);
    textLayout->setContentsMargins(4, 4, 4, 4);
    message = new QLabel;
    message->setObjectName(QStringLiteral("dialogMessage"));
    message->setWordWrap(true);
    message->setTextFormat(Qt::PlainText);
    message->setTextInteractionFlags(Qt::TextSelectableByMouse);
    info = new QLabel;
    info->setWordWrap(true);
    info->setTextFormat(Qt::PlainText);
    info->setTextInteractionFlags(Qt::TextSelectableByMouse);
    info->hide();
    textLayout->addWidget(message);
    textLayout->addWidget(info);
    textLayout->addStretch();
    scroll->setWidget(content);
    row->addWidget(scroll, 1);
    body->addLayout(row, 1);
    detailsToggle = new QToolButton;
    detailsToggle->setObjectName(QStringLiteral("dialogDetailsToggle"));
    detailsToggle->setText(QStringLiteral("显示详细信息"));
    detailsToggle->setCheckable(true);
    detailsToggle->hide();
    body->addWidget(detailsToggle, 0, Qt::AlignLeft);
    details = new QPlainTextEdit;
    details->setObjectName(QStringLiteral("dialogDetails"));
    details->setReadOnly(true);
    details->setMinimumHeight(100);
    details->hide();
    body->addWidget(details, 1);
    connect(detailsToggle, &QToolButton::toggled, this, [this](bool checked) {
        details->setVisible(checked);
        detailsToggle->setText(checked ? QStringLiteral("隐藏详细信息") : QStringLiteral("显示详细信息"));
        if (checked) resize(width(), qMax(height(), 390));
    });
    buttons = new QDialogButtonBox;
    connect(buttons, &QDialogButtonBox::clicked, this, [this](QAbstractButton *clicked) {
        done(int(buttons->standardButton(clicked)));
    });
    body->addWidget(buttons);
    setStandardButtons(Ok);
    setIcon(Information);
    resize(520, 170);
}
void EditorMessageBox::setText(const QString &text) { message->setText(text); }
void EditorMessageBox::setInformativeText(const QString &text) { info->setText(text); info->setVisible(!text.isEmpty()); }
void EditorMessageBox::setDetailedText(const QString &text) {
    details->setPlainText(text);
    detailsToggle->setVisible(!text.isEmpty());
}
void EditorMessageBox::setIcon(Icon icon) {
    symbol->setPixmap(messageIcon(icon, devicePixelRatioF()));
    symbol->setAccessibleName(icon == Question ? QStringLiteral("确认") :
                              icon == Information ? QStringLiteral("提示") : QStringLiteral("警告"));
}
void EditorMessageBox::setStandardButtons(StandardButtons value) {
    buttons->setStandardButtons(QDialogButtonBox::StandardButtons(int(value)));
    for (auto b : {Ok, Yes, Cancel, Save, Discard}) {
        if (auto *widget = buttons->button(QDialogButtonBox::StandardButton(b))) {
            QString text;
            if (b == Ok) text = QStringLiteral("知道了");
            if (b == Yes) text = QStringLiteral("确认");
            if (b == Cancel) text = QStringLiteral("取消");
            if (b == Save) text = QStringLiteral("保存");
            if (b == Discard) text = QStringLiteral("不保存");
            widget->setText(text);
            widget->setAutoDefault(false);
            widget->setDefault(false);
            widget->setCursor(Qt::PointingHandCursor);
        }
    }
    setDefaultButton(value.testFlag(Cancel) ? Cancel : Ok);
}
void EditorMessageBox::setDefaultButton(StandardButton value) {
    for (auto *b : buttons->buttons())
        if (auto *push = qobject_cast<QPushButton *>(b)) push->setDefault(false);
    if (auto *push = buttons->button(QDialogButtonBox::StandardButton(value))) {
        push->setDefault(true);
        push->setFocus();
    }
}
QAbstractButton *EditorMessageBox::button(StandardButton value) const {
    return buttons->button(QDialogButtonBox::StandardButton(value));
}
void EditorMessageBox::reject() {
    done(button(Cancel) ? int(Cancel) : button(Ok) ? int(Ok) : int(NoButton));
}
EditorMessageBox::StandardButton EditorMessageBox::run(QWidget *parent, const QString &title,
        const QString &text, Icon icon, StandardButtons buttons, StandardButton defaultButton) {
    EditorMessageBox dialog(parent);
    dialog.setWindowTitle(title);
    dialog.setText(text);
    dialog.setIcon(icon);
    dialog.setStandardButtons(buttons);
    dialog.setDefaultButton(defaultButton);
    return StandardButton(dialog.exec());
}
EditorMessageBox::StandardButton EditorMessageBox::question(QWidget *p, const QString &title,
        const QString &text, StandardButtons buttons, StandardButton def) {
    return run(p, title, text, Question, buttons, def);
}
EditorMessageBox::StandardButton EditorMessageBox::warning(QWidget *p, const QString &title,
        const QString &text, StandardButtons buttons, StandardButton def) {
    return run(p, title, text, Warning, buttons, def);
}
EditorMessageBox::StandardButton EditorMessageBox::information(QWidget *p, const QString &title, const QString &text) {
    return run(p, title, text, Information, Ok, Ok);
}

namespace {
class FileModel final : public QFileSystemModel {
public:
    using QFileSystemModel::QFileSystemModel;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override {
        const QStringList labels{QStringLiteral("名称"), QStringLiteral("大小"), QStringLiteral("类型"), QStringLiteral("修改时间")};
        if (orientation == Qt::Horizontal && role == Qt::DisplayRole && section >= 0 && section < labels.size())
            return labels.at(section);
        return QFileSystemModel::headerData(section, orientation, role);
    }
};
class FilePicker final : public EditorDialog {
public:
    FilePicker(QWidget *parent, const QString &title, const QString &initial,
               const QString &filter, bool saving)
        : EditorDialog(parent, title), saving(saving) {
        setObjectName(QStringLiteral("editorFileDialog"));
        resize(760, 520);
        auto *navigation = new QHBoxLayout;
        auto *up = new QPushButton(QStringLiteral("上一级"));
        up->setAutoDefault(false);
        navigation->addWidget(up);
        location = new QLineEdit;
        location->setObjectName(QStringLiteral("fileDirectory"));
        location->setAccessibleName(QStringLiteral("文件夹路径"));
        location->installEventFilter(this);
        navigation->addWidget(location, 1);
        auto *go = new QPushButton(QStringLiteral("前往"));
        go->setAutoDefault(false);
        navigation->addWidget(go);
        body->addLayout(navigation);
        model = new FileModel(this);
        model->setReadOnly(true);
        model->setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);
        const auto match = QRegularExpression(QStringLiteral("\\(([^)]+)\\)")).match(filter);
        if (match.hasMatch()) {
            model->setNameFilters(match.captured(1).split(QLatin1Char(' '), Qt::SkipEmptyParts));
            model->setNameFilterDisables(false);
        }
        tree = new QTreeView;
        tree->setObjectName(QStringLiteral("fileTree"));
        tree->setModel(model);
        tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
        tree->setSelectionMode(QAbstractItemView::SingleSelection);
        tree->setSortingEnabled(true);
        tree->sortByColumn(0, Qt::AscendingOrder);
        tree->setColumnWidth(0, 340);
        tree->setRootIsDecorated(false);
        body->addWidget(tree, 1);
        auto *filenameRow = new QHBoxLayout;
        filenameRow->addWidget(new QLabel(QStringLiteral("文件名")));
        filename = new QLineEdit;
        filename->setObjectName(QStringLiteral("fileName"));
        filename->setAccessibleName(QStringLiteral("文件名"));
        filenameRow->addWidget(filename, 1);
        body->addLayout(filenameRow);
        auto *type = new QLabel(filter);
        type->setTextFormat(Qt::PlainText);
        type->setProperty("role", QStringLiteral("hint"));
        body->addWidget(type);
        error = new QLabel;
        error->setObjectName(QStringLiteral("fileError"));
        error->setTextFormat(Qt::PlainText);
        error->setWordWrap(true);
        body->addWidget(error);
        connect(filename, &QLineEdit::textChanged, error, &QLabel::clear);
        connect(location, &QLineEdit::textChanged, error, &QLabel::clear);
        auto *footer = new QHBoxLayout;
        auto *buttons = new QDialogButtonBox;
        buttons->addButton(QStringLiteral("取消"), QDialogButtonBox::RejectRole);
        auto *ok = buttons->addButton(saving ? QStringLiteral("保存") : QStringLiteral("打开"), QDialogButtonBox::AcceptRole);
        ok->setObjectName(QStringLiteral("acceptFile"));
        ok->setDefault(true);
        footer->addWidget(buttons, 1);
        footer->addWidget(new QSizeGrip(this), 0, Qt::AlignBottom);
        body->addLayout(footer);
        connect(buttons, &QDialogButtonBox::accepted, this, &FilePicker::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        connect(up, &QPushButton::clicked, this, [this] {
            auto dir = QDir(currentDirectory); dir.cdUp(); navigate(dir.absolutePath());
        });
        connect(go, &QPushButton::clicked, this, [this] { navigate(location->text()); });
        connect(tree->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex &index) {
            if (!model->isDir(index)) filename->setText(model->fileName(index));
        });
        connect(tree, &QTreeView::activated, this, [this](const QModelIndex &index) {
            if (model->isDir(index)) navigate(model->filePath(index));
            else { filename->setText(model->fileName(index)); accept(); }
        });
        const QFileInfo initialInfo(initial);
        navigate(initial.isEmpty() ? QDir::homePath() : initialInfo.isDir() ? initialInfo.absoluteFilePath() : initialInfo.absolutePath());
        if (!initial.isEmpty() && !initialInfo.isDir()) filename->setText(initialInfo.fileName());
        filename->setFocus();
    }
    QString path;
    void accept() override {
        const auto name = filename->text().trimmed();
        if (name.isEmpty()) { error->setText(QStringLiteral("请选择或输入文件名。")); return; }
        const QFileInfo file(QDir(currentDirectory).absoluteFilePath(name));
        if (file.isDir()) { navigate(file.absoluteFilePath()); return; }
        if (!QFileInfo(file.absolutePath()).isDir()) {
            error->setText(QStringLiteral("目标文件夹不存在，请选择已有文件夹。")); return;
        }
        if (!saving && (!file.isFile() || !file.isReadable())) {
            error->setText(QStringLiteral("文件不存在或不可读取，请重新选择。")); return;
        }
        path = file.absoluteFilePath();
        QDialog::accept();
    }
private:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (watched == location && event->type() == QEvent::KeyPress) {
            auto *key = static_cast<QKeyEvent *>(event);
            if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
                navigate(location->text());
                event->accept();
                return true; // Navigation must not trigger the dialog's default button.
            }
        }
        return EditorDialog::eventFilter(watched, event);
    }
    void navigate(QString directory) {
        if (directory == QStringLiteral("~")) directory = QDir::homePath();
        else if (directory.startsWith(QStringLiteral("~/"))) directory.replace(0, 1, QDir::homePath());
        if (QDir::isRelativePath(directory)) directory = QDir(currentDirectory).absoluteFilePath(directory);
        if (!QFileInfo(directory).isDir()) { error->setText(QStringLiteral("文件夹不存在。")); return; }
        currentDirectory = QDir(directory).absolutePath();
        location->setText(currentDirectory);
        model->setRootPath(currentDirectory);
        tree->setRootIndex(model->index(currentDirectory));
        error->clear();
    }
    bool saving;
    QString currentDirectory;
    QLineEdit *location;
    QLineEdit *filename;
    QLabel *error;
    QTreeView *tree;
    QFileSystemModel *model;
};
}
QString EditorFileDialog::getOpenFileName(QWidget *parent, const QString &title, const QString &directory, const QString &filter) {
    FilePicker picker(parent, title, directory, filter, false);
    return picker.exec() == QDialog::Accepted ? picker.path : QString{};
}
QString EditorFileDialog::getSaveFileName(QWidget *parent, const QString &title, const QString &path, const QString &filter) {
    FilePicker picker(parent, title, path, filter, true);
    return picker.exec() == QDialog::Accepted ? picker.path : QString{};
}
