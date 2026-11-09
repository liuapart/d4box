#include "PointNameDialog.h"
#include "WindowChrome.h"
#include <QDialogButtonBox>
#include <QGuiApplication>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScreen>
#include <QShowEvent>
#include <QVBoxLayout>
#include <QWindow>

PointNameDialog::PointNameDialog(const QString &name, QScreen *screen, QWidget *parent)
    : QDialog(parent, Qt::Tool), targetScreen(screen) {
    setObjectName(QStringLiteral("pointNameDialog"));
    setWindowTitle(QStringLiteral("点位改名"));
    setWindowModality(Qt::ApplicationModal);
    setMinimumWidth(360);
    auto *chrome = new WindowChrome(this, QStringLiteral("点位改名"));
    auto *body = chrome->contentLayout();
    auto *hint = new QLabel(QStringLiteral("名称会同步更新到步骤列表，不改变点位 ID。"), this);
    hint->setWordWrap(true);
    hint->setProperty("role", QStringLiteral("hint"));
    body->addWidget(hint);
    input = new QLineEdit(name, this);
    input->setObjectName(QStringLiteral("pointNameInput"));
    input->setAccessibleName(QStringLiteral("点位名称"));
    input->setPlaceholderText(QStringLiteral("请输入点位名称"));
    body->addWidget(input);
    auto *buttons = new QDialogButtonBox(this);
    auto *cancel = buttons->addButton(QStringLiteral("取消"), QDialogButtonBox::RejectRole);
    auto *save = buttons->addButton(QStringLiteral("确定"), QDialogButtonBox::AcceptRole);
    cancel->setObjectName(QStringLiteral("cancelPointName"));
    save->setObjectName(QStringLiteral("savePointName"));
    save->setDefault(true);
    save->setEnabled(!name.trimmed().isEmpty());
    connect(input, &QLineEdit::textChanged, this, [save](const QString &text) {
        save->setEnabled(!text.trimmed().isEmpty());
    });
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        if (!pointName().isEmpty()) accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    body->addWidget(buttons);
    resize(400, sizeHint().height());

}
QString PointNameDialog::pointName() const { return input->text().trimmed(); }
void PointNameDialog::showEvent(QShowEvent *event) {
    QDialog::showEvent(event);
    input->setFocus(Qt::OtherFocusReason);
    input->selectAll();
}

void PointNameDialog::setVisible(bool visible) {
    QDialog::setVisible(visible);
    if (!visible) return;
    // Center on the overlay's screen, not the toolbar; use logical pixels.
    auto *display = targetScreen ? targetScreen.data() : QGuiApplication::primaryScreen();
    if (display) {
        if (windowHandle()) windowHandle()->setScreen(display);
        const auto area = display->availableGeometry();
        move(area.x() + (area.width() - width()) / 2,
             area.y() + (area.height() - height()) / 2);
    }
}
