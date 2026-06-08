#include "utils/WindowUtil.h"
#include <QDebug>
#include <QFileInfo>
#include "utils/MiscUtil.h"
#include "utils/ProcessUtil.h"
#include "utils/AppUtil.h"
#include "utils/ConfigManager.h"

namespace Util {

    bool isWindowAcceptable(HWND hwnd, bool skipVisibleCheck) {
        // Build filter rules from user config
        WindowFilterRules rules;
        auto blocked = cfg().getBlockedWindows();
        for (const auto& entry : blocked) {
            rules.blockedByUser.append({entry.title, entry.className});
        }
        return WindowEnumerator::isWindowAcceptable(hwnd, skipVisibleCheck, &rules);
    }

    QList<HWND> enumWindows() {
        return WindowEnumerator::enumAllWindows();
    }

    BOOL CALLBACK EnumChildWindowsProc(HWND hwnd, LPARAM lParam) {
        auto* windowList = reinterpret_cast<QList<HWND>*>(lParam);
        windowList->append(hwnd);
        return TRUE;
    }

    QList<HWND> enumChildWindows(HWND hwnd) {
        QList<HWND> list;
        EnumChildWindows(hwnd, EnumChildWindowsProc, reinterpret_cast<LPARAM>(&list));
        return list;
    }

    void closeWindowedPopupClass() {
        HWND hwnd = GetForegroundWindow();
        QString className = getClassName(hwnd);
        if (className == "Xaml_WindowedPopupClass") {
            PostMessage(hwnd, WM_CLOSE, 0, 0);
        }
    }

    void closeStartMenu() {
        HWND hwnd = GetForegroundWindow();
        QString className = getClassName(hwnd);
        if (className == "Windows.UI.Core.CoreWindow") {
            PostMessage(hwnd, WM_CLOSE, 0, 0);
        }
    }

    void closeSystemWindows() {
        closeStartMenu();
        closeWindowedPopupClass();
    }

    QList<HWND> listValidWindows() {
        return WindowEnumerator::enumValidWindows();
    }

    QList<HWND> listValidWindows(const QString& exePath) {
        return WindowEnumerator::enumValidWindows(exePath);
    }

    QList<HWND> findTopWindows(const QString& className, const QString& title) {
        QList<HWND> windows;
        auto pClassName = className.isNull() ? nullptr : LPCWSTR(className.utf16());
        auto pTitle = title.isNull() ? nullptr : LPCWSTR(title.utf16());

        if (HWND hwnd = FindWindow(pClassName, pTitle)) {
            windows << hwnd;
            while ((hwnd = FindWindowEx(nullptr, hwnd, pClassName, pTitle)))
                windows << hwnd;
        }

        return windows;
    }

    HWND topWindowFromPoint(const POINT& pos) {
        HWND hwnd = WindowFromPoint(pos);
        return GetAncestor(hwnd, GA_ROOT);
    }

    HWND getCurrentTaskListThumbnailWnd() {
        const QList<HWND> thumbs = findTopWindows("TaskListThumbnailWnd");
        POINT cursorPos = getCursorPos();
        const auto cursorMonitor = MonitorFromPoint(cursorPos, MONITOR_DEFAULTTONEAREST);
        for (HWND hwnd : thumbs) {
            if (auto monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONULL); monitor == cursorMonitor)
                return hwnd;
        }
        return nullptr;
    }

    bool isTaskbarWindow(HWND hwnd) {
        auto className = Util::getClassName(hwnd);
        return className == QStringLiteral("Shell_TrayWnd") || className == QStringLiteral("Shell_SecondaryTrayWnd");
    }
}
