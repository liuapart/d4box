#pragma once
#include "EditorDialogs.h"
class QLineEdit;
class QScreen;

class PointNameDialog final : public EditorDialog {
public:
    PointNameDialog(const QString &name, QScreen *targetScreen, QWidget *parent);
    QString pointName() const;
protected:
    void showEvent(QShowEvent *event) override;
private:
    QLineEdit *input;
};
