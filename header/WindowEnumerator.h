#ifndef WIN_SWITCHER_WINDOWENUMERATOR_H
#define WIN_SWITCHER_WINDOWENUMERATOR_H

#include <Windows.h>
#include <QList>
#include <QString>
#include "WindowTypes.h"

namespace WindowEnumerator {
    QList<WindowDescriptor> enumAllWindows();
    bool isWindowAcceptable(HWND hwnd, bool skipVisibleCheck = false);
    QList<WindowDescriptor> enumValidWindows();
    QList<WindowDescriptor> enumValidWindows(const QString& exePath);
}

#endif //WIN_SWITCHER_WINDOWENUMERATOR_H
