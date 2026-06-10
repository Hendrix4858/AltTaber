#include "utils/WindowUtil.h"
#include <QDebug>
#include <QFileInfo>
#include "utils/MiscUtil.h"
#include "utils/ProcessUtil.h"
#include "utils/AppUtil.h"
#include "utils/ConfigManager.h"
#include "WindowFilter.h"
#include "WindowDescriptorBuilder.h"

namespace Util {

    bool isWindowAcceptable(HWND hwnd, bool skipVisibleCheck) {
        // System filter
        if (!WindowEnumerator::isWindowAcceptable(hwnd, skipVisibleCheck))
            return false;
        // User filter from config
        WindowFilter filter;
        WindowFilterRule rule;
        auto blocked = cfg().getBlockedWindows();
        for (const auto& entry : blocked) {
            if (!entry.enabled) continue;
            WindowFilterEntry fe;
            fe.title = entry.title;
            fe.className = entry.className;
            fe.processName = entry.processName;
            fe.processPath = entry.processPath;
            rule.entries.append(fe);
        }
        filter.setRules(rule);
        WindowDescriptor desc = WindowDescriptorBuilder::fromHwnd(hwnd);
        QList<WindowDescriptor> list = {desc};
        return !filter.filter(list).isEmpty();
    }

    QList<HWND> enumWindows() {
        auto descriptors = WindowEnumerator::enumAllWindows();
        QList<HWND> hwnds;
        hwnds.reserve(descriptors.size());
        for (const auto& d : descriptors)
            hwnds.append(d.hwnd);
        return hwnds;
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
        auto descriptors = WindowEnumerator::enumValidWindows();
        QList<HWND> hwnds;
        hwnds.reserve(descriptors.size());
        for (const auto& d : descriptors)
            hwnds.append(d.hwnd);
        return hwnds;
    }

    QList<HWND> listValidWindows(const QString& exePath) {
        auto descriptors = WindowEnumerator::enumValidWindows(exePath);
        QList<HWND> hwnds;
        hwnds.reserve(descriptors.size());
        for (const auto& d : descriptors)
            hwnds.append(d.hwnd);
        return hwnds;
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
        const auto className = Util::getClassName(hwnd);

        if (className == QStringLiteral("Shell_TrayWnd"))
            return true;
        if (className == QStringLiteral("Shell_SecondaryTrayWnd"))
            return true;
        if (className == QStringLiteral("TopLevelWindowForOverflowXamlIsland"))
            return true;
        if (className.startsWith(QStringLiteral("Xaml")))
            return true;

        return false;
    }
}
