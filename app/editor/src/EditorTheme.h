#pragma once
#include <QIcon>
class QApplication;
namespace EditorTheme {
enum class Symbol { Add, Grid, Steps, Eye, Pointer, Save, Close };
void install(QApplication &app);
bool validateResources(QString *error = nullptr);
QIcon icon(Symbol symbol, bool primary = false);
}
