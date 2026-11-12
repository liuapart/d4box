#include "EditorDialogs.h"
#include "EditorTheme.h"
#include <QApplication>
#include <QAbstractButton>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileSystemModel>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>
#include <functional>
#include <iostream>

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    EditorTheme::install(app);
    int failures = 0;
    auto check = [&](bool yes, const char *message) {
        if (!yes) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
    };
    QWidget parent;
    parent.setGeometry(40, 40, 380, 50);
    parent.show();
    QString renderDir;
    const auto args = app.arguments();
    const int renderIndex = args.indexOf(QStringLiteral("--render-dir"));
    if (renderIndex >= 0 && renderIndex + 1 < args.size()) {
        renderDir = args[renderIndex + 1];
        QDir().mkpath(renderDir);
    }
    auto inspect = [&](QDialog *dialog) {
        check(dialog && dialog->windowFlags().testFlag(Qt::FramelessWindowHint), "dialog has custom frameless host");
        if (!dialog) return;
        check(dialog->findChild<QWidget *>(QStringLiteral("panelTitleBar")) != nullptr, "shared draggable title exists");
        check(dialog->findChild<QToolButton *>(QStringLiteral("panelClose")) != nullptr, "shared close exists");
        const auto area = parent.screen()->availableGeometry();
        const QPoint expected(area.x() + (area.width() - dialog->width()) / 2,
                              area.y() + (area.height() - dialog->height()) / 2);
        check((dialog->pos() - expected).manhattanLength() < 3, "dialog centered on parent's display");
    };
    // Cocoa may process events during window creation: inspect after show settles.
    auto later = [&](std::function<void(QDialog *)> action) {
        QTimer::singleShot(120, &parent, [&, action] {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            check(dialog != nullptr, "modal dialog exists");
            if (dialog) { inspect(dialog); action(dialog); }
        });
    };
    QTimer watchdog;
    watchdog.setInterval(5000);
    QObject::connect(&watchdog, &QTimer::timeout, [&] {
        check(false, "dialog operation timed out");
        if (auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget())) dialog->reject();
    });
    watchdog.start();
    for (auto choice : {EditorMessageBox::Save, EditorMessageBox::Discard, EditorMessageBox::Cancel}) {
        later([&, choice](QDialog *dialog) {
            auto *box = qobject_cast<EditorMessageBox *>(dialog);
            check(box != nullptr, "unsaved prompt uses editor message class");
            if (!box) { dialog->reject(); return; }
            check(box->button(EditorMessageBox::Save)->text() == QStringLiteral("保存") &&
                  box->button(EditorMessageBox::Discard)->text() == QStringLiteral("不保存") &&
                  box->button(EditorMessageBox::Cancel)->text() == QStringLiteral("取消"), "save buttons localized");
            check(qobject_cast<QPushButton *>(box->button(EditorMessageBox::Cancel))->isDefault(), "unsaved prompt defaults to Cancel");
            if (!renderDir.isEmpty() && choice == EditorMessageBox::Save)
                check(box->grab().save(renderDir + QStringLiteral("/unsaved.png")), "render unsaved prompt");
            box->button(choice)->click();
        });
        const auto answer = EditorMessageBox::question(&parent, QStringLiteral("未保存修改"),
            QStringLiteral("保存“未命名流程”的修改？放弃后未保存内容会丢失。\n保存仅写入草稿，不执行游戏操作。"),
            EditorMessageBox::Save | EditorMessageBox::Discard | EditorMessageBox::Cancel, EditorMessageBox::Cancel);
        check(answer == choice, "Save/Discard/Cancel results remain distinct");
    }
    for (int dismiss : {0, 1, 2}) {
        later([dismiss](QDialog *dialog) {
            if (dismiss == 0) dialog->findChild<QToolButton *>(QStringLiteral("panelClose"))->click();
            else {
                QKeyEvent key(QEvent::KeyPress, dismiss == 1 ? Qt::Key_Escape : Qt::Key_Return, Qt::NoModifier);
                QApplication::sendEvent(dialog, &key);
            }
        });
        check(EditorMessageBox::question(&parent, QStringLiteral("删除步骤"),
              QStringLiteral("删除第 1 步“左键点击”？可通过撤销恢复；不删除点位。")) == EditorMessageBox::Cancel,
              "close Escape and default Enter cancel destructive question");
    }
    for (auto kind : {EditorMessageBox::Question, EditorMessageBox::Information, EditorMessageBox::Warning, EditorMessageBox::Critical}) {
        EditorMessageBox box(&parent);
        box.setWindowTitle(QStringLiteral("删除步骤"));
        box.setText(QStringLiteral("删除第 1 步“左键点击”？可通过撤销恢复；不删除点位。"));
        box.setIcon(kind);
        box.setStandardButtons(EditorMessageBox::Yes | EditorMessageBox::Cancel);
        later([&, kind](QDialog *dialog) {
            auto *symbol = dialog->findChild<QLabel *>(QStringLiteral("dialogSymbol"));
            const auto image = symbol->pixmap().toImage();
            int brightPixels = 0;
            for (int y = 0; y < image.height(); ++y)
                for (int x = 0; x < image.width(); ++x) {
                    const auto color = image.pixelColor(x, y);
                    if (color.alpha() > 180 && color.green() > 160) ++brightPixels;
                }
            check(brightPixels > 100, "message icon has visible high-contrast vector strokes");
            if (!renderDir.isEmpty() && kind == EditorMessageBox::Question)
                check(dialog->grab().save(renderDir + QStringLiteral("/delete-step.png")), "render delete prompt");
            dialog->findChild<QAbstractButton *>(QStringLiteral("panelClose"))->click();
        });
        box.exec();
    }
    EditorMessageBox details(&parent);
    details.setText(QStringLiteral("<b>Literal name</b>"));
    details.setInformativeText(QStringLiteral("校验信息\n").repeated(80));
    details.setDetailedText(QStringLiteral("layouts[0].pointPositions.p1: uncalibrated\n").repeated(80));
    later([&](QDialog *dialog) {
        auto *label = dialog->findChild<QLabel *>(QStringLiteral("dialogMessage"));
        check(label->textFormat() == Qt::PlainText, "workflow text cannot inject rich markup");
        auto *scroll = dialog->findChild<QScrollArea *>();
        check(scroll->verticalScrollBar()->maximum() > 0, "long message content scrolls");
        dialog->findChild<QToolButton *>(QStringLiteral("dialogDetailsToggle"))->click();
        check(dialog->findChild<QPlainTextEdit *>(QStringLiteral("dialogDetails"))->isVisible(), "validation details expandable");
        dialog->reject();
    });
    details.exec();

    QTemporaryDir temp;
    check(temp.isValid(), "temporary file fixture exists");
    const QString path = temp.filePath(QStringLiteral("有空格 workflow.json"));
    QFile file(path);
    check(file.open(QIODevice::WriteOnly), "fixture writable");
    file.write("{\"unchanged\":true}"); file.close();
    check(QDir(temp.path()).mkdir(QStringLiteral("子文件夹")), "navigation fixture exists");
    const auto filter = QStringLiteral("流程 JSON (*.json)");
    later([&](QDialog *dialog) {
        auto *name = dialog->findChild<QLineEdit *>(QStringLiteral("fileName"));
        name->setText(QStringLiteral("missing.json"));
        dialog->accept();
        check(dialog->isVisible() && !dialog->findChild<QLabel *>(QStringLiteral("fileError"))->text().isEmpty(), "missing file stays in chooser with inline error");
        name->setText(QFileInfo(path).fileName());
        if (!renderDir.isEmpty()) check(dialog->grab().save(renderDir + QStringLiteral("/file-picker.png")), "render custom file picker");
        dialog->accept();
    });
    check(EditorFileDialog::getOpenFileName(&parent, QStringLiteral("打开配置 JSON"), temp.path(), filter) == path,
          "custom open returns Unicode/spaced path");
    later([&](QDialog *dialog) {
        dialog->findChild<QLineEdit *>(QStringLiteral("fileName"))->setText(QStringLiteral("not-a-directory/new.json"));
        dialog->accept();
        check(dialog->isVisible(), "save with missing directory is not accepted");
        dialog->findChild<QLineEdit *>(QStringLiteral("fileName"))->setText(QFileInfo(path).fileName());
        dialog->accept();
    });
    check(EditorFileDialog::getSaveFileName(&parent, QStringLiteral("保存草稿"), path, filter) == path,
          "save chooser leaves overwrite decision to caller");
    check(file.open(QIODevice::ReadOnly) && file.readAll() == QByteArray("{\"unchanged\":true}"), "choosing save never overwrites file");
    file.close();
    later([](QDialog *dialog) { dialog->reject(); });
    check(EditorFileDialog::getSaveFileName(&parent, QStringLiteral("保存草稿"), path, filter).isEmpty(), "file chooser cancel returns no path");
    later([&](QDialog *dialog) {
        auto *tree = dialog->findChild<QTreeView *>(QStringLiteral("fileTree"));
        auto *model = qobject_cast<QFileSystemModel *>(tree->model());
        check(model->nameFilters() == QStringList{QStringLiteral("*.json")}, "file filter is respected");
        dialog->findChild<QToolButton *>(QStringLiteral("panelClose"))->click();
    });
    check(EditorFileDialog::getOpenFileName(&parent, QStringLiteral("打开配置"), temp.path(), filter).isEmpty(), "file chooser X cancels");
    later([&](QDialog *dialog) {
        auto *location = dialog->findChild<QLineEdit *>(QStringLiteral("fileDirectory"));
        location->setText(temp.filePath(QStringLiteral("子文件夹")));
        location->setFocus();
        QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
        QApplication::sendEvent(location, &enter);
        auto *tree = dialog->findChild<QTreeView *>(QStringLiteral("fileTree"));
        auto *model = qobject_cast<QFileSystemModel *>(tree->model());
        check(dialog->isVisible() && model->rootPath() == temp.filePath(QStringLiteral("子文件夹")),
              "Enter in directory field navigates without accepting existing filename");
        dialog->reject();
    });
    check(EditorFileDialog::getOpenFileName(&parent, QStringLiteral("打开配置"), path, filter).isEmpty(), "navigation is not file selection");
    std::cout << "Editor dialogs: " << failures << " failures\n";
    return failures ? 1 : 0;
}
