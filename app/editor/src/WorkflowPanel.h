#pragma once

#include "WorkflowDocument.h"
#include <QWidget>
#include <functional>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
class QTabWidget;

// This tool window edits document data only. It has no execution/input backend.
class WorkflowPanel : public QWidget {
  public:
    using Mutation = std::function<void(const QString &, const std::function<void()> &)>;
    WorkflowPanel(WorkflowDocument *document, Mutation mutate, QWidget *parent);
    void refresh();
    void showPoints(const QString &id = {});
    void showLayout();
    void setReferenceCrop(const QRectF &rect);
    std::function<void(const QString &)> locatePoint;
    std::function<void()> confirmReference;
    std::function<void(const QRectF &)> cropChanged;

  private:
    bool eventFilter(QObject *watched, QEvent *event) override;
    QWidget *titleBar = nullptr;
    bool movingPanel = false;
    QPoint dragOffset;
    void buildSteps();
    void buildPoints();
    void buildChecks();
    void buildLayout();
    void loadStep();
    void loadPoint();
    void loadCheck();
    void editStep(const QString &field, const QJsonValue &value);
    void editCheck(const QString &field, const QJsonValue &value);
    void editSource();
    void addStep(const QString &action, const QString &button = {});
    QString selectedId(QListWidget *list) const;
    WorkflowDocument *doc;
    Mutation mutate;
    bool refreshing = false;
    QTabWidget *tabs;
    QLabel *validation;
    QComboBox *sourceMode;
    QSpinBox *maxItems;
    QSpinBox *totalTimeout;
    QLineEdit *excluded;
    QList<QCheckBox *> qualities;
    QListWidget *steps;
    QLineEdit *stepName;
    QComboBox *action;
    QComboBox *target;
    QComboBox *quality;
    QComboBox *risk;
    QSpinBox *delay;
    QSpinBox *timeout;
    QComboBox *completion;
    QComboBox *stepCheck;
    QPushButton *deleteStep;
    QPushButton *up;
    QPushButton *down;
    QPushButton *locate;
    QListWidget *points;
    QLineEdit *pointName;
    QDoubleSpinBox *pointX;
    QDoubleSpinBox *pointY;
    QLabel *pointState;
    QPushButton *deletePoint;
    QPushButton *placePoint;
    QListWidget *checks;
    QLineEdit *checkName;
    QComboBox *checkPoint;
    QLineEdit *color;
    QSpinBox *tolerance;
    QSpinBox *samples;
    QComboBox *checkMode;
    QComboBox *entryCheck;
    QPushButton *deleteCheck;
    QComboBox *layouts;
    QDoubleSpinBox *inventory[4];
    QDoubleSpinBox *crop[4];
    QLineEdit *description;
};
