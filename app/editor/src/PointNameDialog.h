#pragma once
#include <QDialog>
#include <QPointer>
class QLineEdit;
class QScreen;

class PointNameDialog final : public QDialog {
public:
    PointNameDialog(const QString &name, QScreen *targetScreen, QWidget *parent);
    QString pointName() const;
    void setVisible(bool visible) override;
protected:
    void showEvent(QShowEvent *event) override;
private:
    QLineEdit *input;
    QPointer<QScreen> targetScreen;
};
