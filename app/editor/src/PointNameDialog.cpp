#include "PointNameDialog.h"
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QShowEvent>
#include <QVBoxLayout>

PointNameDialog::PointNameDialog(const QString &name, QScreen *screen, QWidget *parent)
    : EditorDialog(parent, QStringLiteral("点位改名"), screen) {
    setObjectName(QStringLiteral("pointNameDialog"));
    setWindowTitle(QStringLiteral("点位改名"));
    setWindowModality(Qt::ApplicationModal);
    setMinimumWidth(360);
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
