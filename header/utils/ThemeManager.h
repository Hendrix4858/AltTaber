#ifndef WIN_SWITCHER_THEMEMANAGER_H
#define WIN_SWITCHER_THEMEMANAGER_H

#include <QColor>
#include <QObject>
#include <QSettings>
#include "ConfigManager.h"

enum Theme {
    Dark,
    Light,
    System,
    ThemeEnumCount
};

struct ThemeColors {
    QColor bgColor;
    QColor panelColor;
    QColor textColor;
    QColor highlightColor;
    QColor borderColor;
    QColor inputBg;
    QColor accentColor;
    QColor widgetBg;
    QColor delegateSelected;
    QColor delegateHover;
    QColor delegateHoverUnselected;
    QColor badgeBg;
    QColor badgeText;
    QColor trayBg;
    QColor trayText;
    QColor trayBorder;
    QColor traySelected;
    QColor updateBg;
    QColor updateText;
    QColor disabledText;
};

class ThemeManager : public QObject {
    Q_OBJECT
public:
    ThemeManager(const ThemeManager&) = delete;
    ThemeManager& operator=(const ThemeManager&) = delete;

    static ThemeManager& instance() {
        static ThemeManager inst;
        return inst;
    }

    static Theme resolveTheme() {
        int saved = cfg().get("Theme", Dark).toInt();
        if (saved == System)
            return detectSystemTheme();
        return static_cast<Theme>(saved);
    }

    static Theme detectSystemTheme() {
        QSettings reg(
            "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
            QSettings::NativeFormat);
        int value = reg.value("AppsUseLightTheme", 1).toInt();
        return value == 1 ? Light : Dark;
    }

    static const ThemeColors& current() {
        return instance().colors(resolveTheme());
    }

    static const ThemeColors& colors(Theme theme) {
        static const ThemeColors dark = {
            .bgColor           = QColor(35, 35, 35),
            .panelColor        = QColor(45, 45, 45),
            .textColor         = QColor(220, 220, 220),
            .highlightColor    = QColor(60, 60, 60),
            .borderColor       = QColor(25, 25, 25),
            .inputBg           = QColor(50, 50, 50),
            .accentColor       = QColor(0, 120, 212),
            .widgetBg          = QColor(25, 25, 25, 100),
            .delegateSelected  = QColor(60, 60, 60, 180),
            .delegateHover     = QColor(60, 60, 60, 100),
            .delegateHoverUnselected = QColor(55, 55, 55),
            .badgeBg           = QColor(133, 114, 97),
            .badgeText         = QColor(214, 192, 171),
            .trayBg            = QColor(45, 45, 45),
            .trayText          = QColor(220, 220, 220),
            .trayBorder        = QColor(0, 0, 0),
            .traySelected      = QColor(60, 60, 60),
            .updateBg          = QColor(45, 45, 45),
            .updateText        = QColor(220, 220, 220),
            .disabledText      = QColor(85, 85, 85),
        };
        static const ThemeColors light = {
            .bgColor           = QColor(245, 245, 245),
            .panelColor        = QColor(255, 255, 255),
            .textColor         = QColor(0, 0, 0),
            .highlightColor    = QColor(0, 120, 212),
            .borderColor       = QColor(200, 200, 200),
            .inputBg           = QColor(255, 255, 255),
            .accentColor       = QColor(0, 120, 212),
            .widgetBg          = QColor(240, 240, 240, 180),
            .delegateSelected  = QColor(0, 120, 212, 60),
            .delegateHover     = QColor(0, 120, 212, 30),
            .delegateHoverUnselected = QColor(230, 230, 230),
            .badgeBg           = QColor(200, 180, 160),
            .badgeText         = QColor(80, 60, 40),
            .trayBg            = QColor(255, 255, 255),
            .trayText          = QColor(0, 0, 0),
            .trayBorder        = QColor(180, 180, 180),
            .traySelected      = QColor(0, 120, 212),
            .updateBg          = QColor(255, 255, 255),
            .updateText        = QColor(0, 0, 0),
            .disabledText      = QColor(180, 180, 180),
        };
        return theme == Light ? light : dark;
    }

    static void applyTheme() {
        emit instance().themeChanged();
    }

signals:
    void themeChanged();

private:
    ThemeManager() : QObject(nullptr) {}
};

#endif //WIN_SWITCHER_THEMEMANAGER_H
