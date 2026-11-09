#pragma once
#include <QObject>
#include <QPoint>
#include <QString>
class QWidget;
class QVBoxLayout;

// Shared rounded frame and draggable title. Host owns close semantics.
class WindowChrome final : public QObject {
public:
    WindowChrome(QWidget *host, const QString &caption);
    QVBoxLayout *contentLayout() const { return content; }
private:
    bool eventFilter(QObject *watched, QEvent *event) override;
    QWidget *host;
    QWidget *titleBar = nullptr;
    QVBoxLayout *content = nullptr;
    bool movingPanel = false;
    QPoint dragOffset;
};
