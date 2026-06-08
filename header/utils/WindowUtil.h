#ifndef WIN_SWITCHER_WINDOWUTIL_H
#define WIN_SWITCHER_WINDOWUTIL_H

#include <Windows.h>
#include <QString>
#include <QList>
#include "WindowEnumerator.h"

namespace Util {
    bool isWindowAcceptable(HWND hwnd, bool skipVisibleCheck = false);
    QList<HWND> enumWindows();
    QList<HWND> enumChildWindows(HWND hwnd);
    QList<HWND> listValidWindows();
    QList<HWND> listValidWindows(const QString& exePath);
    QList<HWND> findTopWindows(const QString& className, const QString& title = QString());
    void closeSystemWindows();
    HWND topWindowFromPoint(const POINT& pos);
    HWND getCurrentTaskListThumbnailWnd();
    bool isTaskbarWindow(HWND hwnd);
}

#endif //WIN_SWITCHER_WINDOWUTIL_H
