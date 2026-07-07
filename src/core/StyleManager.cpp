#include "core/StyleManager.h"
#include "core/ThemeManager.h"
#include <QApplication>
#include <QFile>
#include <QStyle>

void StyleManager::applyTheme(const ThemeColors& colors) {
    QString qss = loadStyleSheet();
    if (qss.isEmpty())
        return;

    qss.replace("%TEXT_COLOR%",       colors.textColor.name());
    qss.replace("%BORDER_COLOR%",     colors.borderColor.name());
    qss.replace("%TRAY_BG%",          colors.trayBg.name());
    qss.replace("%TRAY_TEXT%",        colors.trayText.name());
    qss.replace("%TRAY_BORDER%",      colors.trayBorder.name());
    qss.replace("%TRAY_SELECTED%",    colors.traySelected.name());

    qApp->setStyleSheet(qss);
}

QString StyleManager::loadStyleSheet() {
    QFile file(":/qss/style.qss");
    if (file.open(QFile::ReadOnly | QFile::Text))
        return QString::fromUtf8(file.readAll());
    return {};
}
