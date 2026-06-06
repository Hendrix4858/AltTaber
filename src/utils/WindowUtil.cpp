#include "utils/WindowUtil.h"
#include <QDebug>
#include <QFileInfo>
#include <shlobj_core.h>
#include "utils/MiscUtil.h"
#include "utils/ProcessUtil.h"
#include "utils/AppUtil.h"
#include "utils/ConfigManager.h"

namespace Util {
    static bool isBlockedByUser(HWND hwnd) {
        auto blocked = cfg().getBlockedWindows();
        if (blocked.isEmpty()) return false;

        QString title = getWindowTitle(hwnd);
        QString className = getClassName(hwnd);

        for (const auto& entry : blocked) {
            bool titleMatch = entry.title.isEmpty() || title.contains(entry.title, Qt::CaseInsensitive);
            bool classMatch = entry.className.isEmpty() || className.contains(entry.className, Qt::CaseInsensitive);
            if (titleMatch && classMatch)
                return true;
        }
        return false;
    }

    bool isWindowAcceptable(HWND hwnd, bool skipVisibleCheck) {
        static const QStringList BlackList_ClassName = {
            "Progman",
            "Windows.UI.Core.CoreWindow",
            "CEF-OSC-WIDGET",
            "WorkerW",
            "Shell_TrayWnd"
        };
        static const QStringList BlackList_ExePath = {
            R"(C:\Windows\System32\wscript.exe)"
        };
        static const QStringList BlackList_FileName = {
            "Nahimic3.exe",
            "Follower.exe",
            "QQ Follower.exe"
        };
        LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
        QString className;

        if ((skipVisibleCheck || IsWindowVisible(hwnd))
            && !isWindowCloaked(hwnd)
            && (!GetWindow(hwnd, GW_OWNER) || exStyle & WS_EX_APPWINDOW)
            && (exStyle & WS_EX_TOOLWINDOW) == 0
            && GetWindowTextLength(hwnd) > 0
            && (className = getClassName(hwnd)).size() > 0
            && !BlackList_ClassName.contains(className)
            && !className.startsWith("imestatuspop_classname{")
            && !isBlockedByUser(hwnd)
        ) {
            auto path = getWindowProcessPath(hwnd);
            if (!BlackList_ExePath.contains(path) && !BlackList_FileName.contains(QFileInfo(path).fileName()))
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

    QList<HWND> enumWindows() {
        QList<HWND> list;
        EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&list));
        return list;
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
        qInfo() << "List valid windows";
        static const bool isUserAdmin = IsUserAnAdmin();
        using namespace AppUtil;
        QList<HWND> list;
        const auto winList = Util::enumWindows();

        for (auto hwnd: winList) {
            if (!hwnd) continue;
            if (!isUserAdmin && isWindowElevated(hwnd)) {
                qInfo() << "Ignore elevated:" << hwnd << getWindowTitle(hwnd);
                continue;
            }



            list << hwnd;
        }
        return list;
    }

    QList<HWND> listValidWindows(const QString& exePath) {
        QList<HWND> windows;
        const auto winList = listValidWindows();
        for (auto hwnd: winList) {
            if (getWindowProcessPath(hwnd).compare(exePath, Qt::CaseInsensitive) == 0)
                windows << hwnd;
        }
        return windows;
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
        for (HWND hwnd: thumbs) {
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
