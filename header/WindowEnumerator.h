#ifndef WIN_SWITCHER_WINDOWENUMERATOR_H
#define WIN_SWITCHER_WINDOWENUMERATOR_H

#include <Windows.h>
#include <QList>
#include <QString>

struct WindowFilterRules {
    QStringList blockedClassNames;
    QStringList blockedExePaths;
    QStringList blockedFileNames;
    QList<std::pair<QString, QString>> blockedByUser; // title, class patterns

    bool skipElevatedIfNotAdmin = true;
};

namespace WindowEnumerator {
    QList<HWND> enumAllWindows();
    bool isWindowAcceptable(HWND hwnd, bool skipVisibleCheck = false,
                            const WindowFilterRules* filterRules = nullptr);
    QList<HWND> enumValidWindows(const WindowFilterRules* filterRules = nullptr);
    QList<HWND> enumValidWindows(const QString& exePath,
                                 const WindowFilterRules* filterRules = nullptr);
}

#endif //WIN_SWITCHER_WINDOWENUMERATOR_H
