#pragma once

#include <QString>

class QApplication;

namespace StyleManager {

QString darkStyleSheet();
void applyDarkTheme(QApplication &app);

} // namespace StyleManager
