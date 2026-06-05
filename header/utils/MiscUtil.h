#ifndef WIN_SWITCHER_MISCUTIL_H
#define WIN_SWITCHER_MISCUTIL_H

#include <Windows.h>
#include <QString>
#include <dwmapi.h>

namespace Util {
    QString getWindowTitle(HWND hwnd);
    QString getClassName(HWND hwnd);
    bool isWindowCloaked(HWND hwnd);
    QString getFileDescription(const QString& path);
    QChar getDisplayFirstLetter(const QString& text);
    bool isKeyPressed(int vkey);
    bool setWindowRoundCorner(HWND hwnd, DWM_WINDOW_CORNER_PREFERENCE pvAttribute = DWMWCP_ROUND);
    POINT getCursorPos();
    bool isTopMost(HWND hwnd);
}

#endif //WIN_SWITCHER_MISCUTIL_H
