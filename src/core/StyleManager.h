#ifndef WIN_SWITCHER_STYLEMANAGER_H
#define WIN_SWITCHER_STYLEMANAGER_H

#include <QString>

struct ThemeColors;

class StyleManager {
public:
    static void applyTheme(const ThemeColors& colors);

private:
    static QString loadStyleSheet();
};

#endif //WIN_SWITCHER_STYLEMANAGER_H
