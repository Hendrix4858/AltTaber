#include "WindowEnumerator.h"
#include "WindowDescriptorBuilder.h"
#include "utils/Util.h"
#include "utils/WindowUtil.h"
#include "core/ConfigManager.h"
#include <QDebug>
#include <QFileInfo>


namespace WindowEnumerator {

    bool isWindowAcceptable(HWND hwnd, bool skipVisibleCheck, bool skipCloakedCheck) {
        LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);

        bool visibleOk = skipVisibleCheck || IsWindowVisible(hwnd);
        bool cloakedOk = skipCloakedCheck || !Util::isWindowCloaked(hwnd);

        HWND owner = GetWindow(hwnd, GW_OWNER);
        bool isDelphiAppWindow = owner && Util::getClassName(owner) == QStringLiteral("TApplication");
        bool ownerOk = !owner || (exStyle & WS_EX_APPWINDOW) || isDelphiAppWindow;

        if (visibleOk && cloakedOk && ownerOk
            && (exStyle & WS_EX_TOOLWINDOW) == 0
            && (GetWindowTextLength(hwnd) > 0 || (exStyle & WS_EX_APPWINDOW))
        ) {
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

    BOOL CALLBACK EnumWindowsProcAll(HWND hwnd, LPARAM lParam) {
        if (isWindowAcceptable(hwnd, false, true)) {
            auto* windowList = reinterpret_cast<QList<HWND>*>(lParam);
            windowList->append(hwnd);
        }
        return TRUE;
    }

    QList<WindowDescriptor> enumAllWindows() {
        return enumAllWindows(false);
    }

    QList<WindowDescriptor> enumAllWindows(bool includeCloaked) {
        Util::clearIdentityCache();
        QList<HWND> hwnds;
        if (includeCloaked)
            EnumWindows(EnumWindowsProcAll, reinterpret_cast<LPARAM>(&hwnds));
        else
            EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&hwnds));

        QList<WindowDescriptor> result;
        result.reserve(hwnds.size());
        for (auto hwnd : hwnds) {
            result.append(WindowDescriptorBuilder::fromHwnd(hwnd));
        }
        return result;
    }

    QList<WindowDescriptor> enumValidWindows() {
        qDebug() << "List valid windows";
        static const bool isUserAdmin = Util::isUserAdmin();
        const auto descriptors = enumAllWindows();
        QList<WindowDescriptor> result;
        result.reserve(descriptors.size());

        for (const auto& desc : descriptors) {
            if (!desc.hwnd) continue;

            if (!isUserAdmin && Util::isWindowElevated(desc.hwnd)) {
                qDebug() << "Ignore elevated:" << desc.hwnd << desc.title;
                continue;
            }

            result.append(desc);
        }
        return result;
    }

    QList<WindowDescriptor> enumValidWindows(const QString& exePath) {
        const auto descriptors = enumValidWindows();
        QList<WindowDescriptor> result;
        result.reserve(descriptors.size());

        for (const auto& desc : descriptors) {
            if (desc.processPath.compare(exePath, Qt::CaseInsensitive) == 0)
                result.append(desc);
        }
        return result;
    }

    QList<WindowDescriptor> enumValidWindows(int virtualDesktopScope) {
        auto scope = static_cast<VirtualDesktopScope>(virtualDesktopScope);

        // CurrentDesktop: exclude cloaked windows (other VDs are cloaked by DWM)
        // AllDesktops: include all windows regardless of cloaked status
        auto descriptors = (scope == VirtualDesktopScope::CurrentDesktop)
            ? enumAllWindows(false)
            : enumAllWindows(true);

        static const bool isUserAdmin = Util::isUserAdmin();
        QList<WindowDescriptor> result;
        result.reserve(descriptors.size());

        for (const auto& desc : descriptors) {
            if (!desc.hwnd) continue;

            if (!isUserAdmin && Util::isWindowElevated(desc.hwnd))
                continue;

            result.append(desc);
        }
        return result;
    }

} // namespace WindowEnumerator
