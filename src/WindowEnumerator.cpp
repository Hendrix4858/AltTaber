#include "WindowEnumerator.h"
#include "utils/Util.h"
#include <QDebug>
#include <QFileInfo>
#include <shlobj_core.h>

namespace WindowEnumerator {

    static bool isBlockedByUser(HWND hwnd, const WindowFilterRules* rules) {
        if (!rules) return false;
        if (rules->blockedByUser.isEmpty()) return false;

        QString title = Util::getWindowTitle(hwnd);
        QString className = Util::getClassName(hwnd);

        for (const auto& [titlePattern, classPattern] : rules->blockedByUser) {
            bool titleMatch = titlePattern.isEmpty() || title.contains(titlePattern, Qt::CaseInsensitive);
            bool classMatch = classPattern.isEmpty() || className.contains(classPattern, Qt::CaseInsensitive);
            if (titleMatch && classMatch)
                return true;
        }
        return false;
    }

    bool isWindowAcceptable(HWND hwnd, bool skipVisibleCheck,
                            const WindowFilterRules* filterRules) {
        QStringList blockedClassNames = {
            "Progman",
            "Windows.UI.Core.CoreWindow",
            "CEF-OSC-WIDGET",
            "WorkerW",
            "Shell_TrayWnd"
        };
        QStringList blockedExePaths = {
            R"(C:\Windows\System32\wscript.exe)"
        };
        QStringList blockedFileNames = {
            "Nahimic3.exe",
            "Follower.exe",
            "QQ Follower.exe"
        };

        if (filterRules) {
            if (!filterRules->blockedClassNames.isEmpty())
                blockedClassNames.append(filterRules->blockedClassNames);
            if (!filterRules->blockedExePaths.isEmpty())
                blockedExePaths.append(filterRules->blockedExePaths);
            if (!filterRules->blockedFileNames.isEmpty())
                blockedFileNames.append(filterRules->blockedFileNames);
        }

        LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
        QString className;

        if ((skipVisibleCheck || IsWindowVisible(hwnd))
            && !Util::isWindowCloaked(hwnd)
            && (!GetWindow(hwnd, GW_OWNER) || exStyle & WS_EX_APPWINDOW)
            && (exStyle & WS_EX_TOOLWINDOW) == 0
            && GetWindowTextLength(hwnd) > 0
            && (className = Util::getClassName(hwnd)).size() > 0
            && !blockedClassNames.contains(className)
            && !className.startsWith("imestatuspop_classname{")
            && !isBlockedByUser(hwnd, filterRules)
        ) {
            auto path = Util::getWindowProcessPath(hwnd);
            if (!blockedExePaths.contains(path) && !blockedFileNames.contains(QFileInfo(path).fileName()))
                return true;
        }
        return false;
    }

    BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
        if (isWindowAcceptable(hwnd)) {
            auto* windowList = reinterpret_cast<QList<HWND>*>(lParam);
            windowList->append(hwnd);
        }
        return TRUE;
    }

    QList<HWND> enumAllWindows() {
        QList<HWND> list;
        EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&list));
        return list;
    }

    QList<HWND> enumValidWindows(const WindowFilterRules* filterRules) {
        qDebug() << "List valid windows";
        static const bool isUserAdmin = IsUserAnAdmin();
        QList<HWND> list;
        const auto winList = enumAllWindows();

        for (auto hwnd : winList) {
            if (!hwnd) continue;

            bool skipElevated = filterRules ? filterRules->skipElevatedIfNotAdmin : true;
            if (!isUserAdmin && skipElevated && Util::isWindowElevated(hwnd)) {
                qDebug() << "Ignore elevated:" << hwnd << Util::getWindowTitle(hwnd);
                continue;
            }

            list << hwnd;
        }
        return list;
    }

    QList<HWND> enumValidWindows(const QString& exePath, const WindowFilterRules* filterRules) {
        QList<HWND> windows;
        const auto winList = enumValidWindows(filterRules);
        for (auto hwnd : winList) {
            if (Util::getWindowProcessPath(hwnd).compare(exePath, Qt::CaseInsensitive) == 0)
                windows << hwnd;
        }
        return windows;
    }

} // namespace WindowEnumerator
