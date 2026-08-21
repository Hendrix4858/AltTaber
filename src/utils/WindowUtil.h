#ifndef WIN_SWITCHER_WINDOWUTIL_H
#define WIN_SWITCHER_WINDOWUTIL_H

#include <Windows.h>
#include <QString>
#include <QList>
#include "WindowEnumerator.h"
#include "WindowTypes.h"

namespace Util {
    bool isWindowAllowed(HWND hwnd, bool skipVisibleCheck = false);
    QList<HWND> enumChildWindows(HWND hwnd);
    QList<HWND> listValidWindows();
    QList<HWND> findTopWindows(const QString& className, const QString& title = QString());
    void closeSystemWindows();
    HWND topWindowFromPoint(const POINT& pos);
    HWND getCurrentTaskListThumbnailWnd();
    bool isTaskbarWindow(HWND hwnd);
    AppIdentity resolveIdentity(HWND hwnd, const QString& knownAumid = {});
    void clearIdentityCache();
}

#endif //WIN_SWITCHER_WINDOWUTIL_H
