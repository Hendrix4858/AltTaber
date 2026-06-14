#ifndef WIN_SWITCHER_THEMEMANAGER_H
#define WIN_SWITCHER_THEMEMANAGER_H

#include <QColor>
#include <QObject>

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

    static ThemeManager& instance();

    static Theme resolveTheme();
    static Theme detectSystemTheme();

    static const ThemeColors& current();
    static const ThemeColors& colors(Theme theme);

    static void applyTheme();

signals:
    void themeChanged();

private:
    ThemeManager() : QObject(nullptr) {}
};

#endif //WIN_SWITCHER_THEMEMANAGER_H
