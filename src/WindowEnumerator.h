#ifndef WIN_SWITCHER_WINDOWENUMERATOR_H
#define WIN_SWITCHER_WINDOWENUMERATOR_H

#include <Windows.h>
#include <QList>
#include <QString>
#include "WindowTypes.h"

namespace WindowEnumerator {
    QList<WindowDescriptor> enumAllWindows();
    QList<WindowDescriptor> enumAllWindows(bool includeCloaked);
    bool isWindowAcceptable(HWND hwnd, bool skipVisibleCheck = false, bool skipCloakedCheck = false);
    bool isLikelyUtilityWindow(HWND hwnd);
    QList<WindowDescriptor> enumValidWindows();
    QList<WindowDescriptor> enumValidWindows(const QString& exePath);
    QList<WindowDescriptor> enumValidWindows(int virtualDesktopScope);
}

#endif //WIN_SWITCHER_WINDOWENUMERATOR_H
