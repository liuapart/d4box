#pragma once

#include <QDialog>
#include <QMessageBox>
#include <QPointer>

class QAbstractButton;
class QDialogButtonBox;
class QLabel;
class QPlainTextEdit;
class QScreen;
class QToolButton;
class QVBoxLayout;

// Shared modal host: rounded chrome, explicit screen centering, safe close.
class EditorDialog : public QDialog {
public:
    EditorDialog(QWidget *parent, const QString &title, QScreen *targetScreen = nullptr);
    void setVisible(bool visible) override;
protected:
    QVBoxLayout *body;
private:
    QPointer<QScreen> targetScreen;
};

class EditorMessageBox final : public EditorDialog {
    Q_OBJECT
public:
    using StandardButton = QMessageBox::StandardButton;
    using StandardButtons = QMessageBox::StandardButtons;
    using Icon = QMessageBox::Icon;
    static constexpr auto NoButton = QMessageBox::NoButton, Ok = QMessageBox::Ok,
                          Yes = QMessageBox::Yes, Cancel = QMessageBox::Cancel,
                          Save = QMessageBox::Save, Discard = QMessageBox::Discard;
    static constexpr auto Information = QMessageBox::Information, Question = QMessageBox::Question,
                          Warning = QMessageBox::Warning, Critical = QMessageBox::Critical;
    explicit EditorMessageBox(QWidget *parent = nullptr);
    void setText(const QString &text);
    void setInformativeText(const QString &text);
    void setDetailedText(const QString &text);
    void setIcon(Icon icon);
    void setStandardButtons(StandardButtons buttons);
    void setDefaultButton(StandardButton button);
    QAbstractButton *button(StandardButton button) const;
    void reject() override;
    static StandardButton question(QWidget *parent, const QString &title, const QString &text,
                                   StandardButtons buttons = Yes | Cancel, StandardButton defaultButton = Cancel);
    static StandardButton warning(QWidget *parent, const QString &title, const QString &text,
                                  StandardButtons buttons = Ok, StandardButton defaultButton = Ok);
    static StandardButton information(QWidget *parent, const QString &title, const QString &text);
private:
    static StandardButton run(QWidget *, const QString &, const QString &, Icon, StandardButtons, StandardButton);
    QLabel *message;
    QLabel *info;
    QLabel *symbol;
    QToolButton *detailsToggle;
    QPlainTextEdit *details;
    QDialogButtonBox *buttons;
};

// File selection only. No file mutation happens until the caller validates and
// confirms a save/overwrite. This also avoids native chooser-owned message boxes.
namespace EditorFileDialog {
QString getOpenFileName(QWidget *parent, const QString &title, const QString &directory, const QString &filter);
QString getSaveFileName(QWidget *parent, const QString &title, const QString &path, const QString &filter);
}
