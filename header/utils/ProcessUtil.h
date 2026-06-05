#ifndef WIN_SWITCHER_PROCESSUTIL_H
#define WIN_SWITCHER_PROCESSUTIL_H

#include <Windows.h>
#include <QString>
#include <QList>

namespace Util {
    bool isProcessElevated(HANDLE hProcess);
    bool isWindowElevated(HWND hwnd);
    QString getProcessPath(DWORD pid);
    QString getWindowProcessPath(HWND hwnd);
    DWORD findProcessByPath(const QString& exePath);
    QList<QString> getChildProcessPaths(DWORD parentPID);
    QList<QString> getChildProcessPaths(const QString& exePath);
    void switchToWindow(HWND hwnd, bool force = false);
    void bringWindowToTop(HWND hwnd, HWND hWndInsertAfter = HWND_TOPMOST);
}

#endif //WIN_SWITCHER_PROCESSUTIL_H
