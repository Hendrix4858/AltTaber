#include "WindowEnumerator.h"
#include "WindowDescriptorBuilder.h"
#include "utils/Util.h"
#include <QDebug>
#include <QFileInfo>


namespace WindowEnumerator {

    bool isWindowAcceptable(HWND hwnd, bool skipVisibleCheck) {
        LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);

        if ((skipVisibleCheck || IsWindowVisible(hwnd))
            && !Util::isWindowCloaked(hwnd)
            && (!GetWindow(hwnd, GW_OWNER) || exStyle & WS_EX_APPWINDOW)
            && (exStyle & WS_EX_TOOLWINDOW) == 0
            && GetWindowTextLength(hwnd) > 0
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

    QList<WindowDescriptor> enumAllWindows() {
        QList<HWND> hwnds;
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

} // namespace WindowEnumerator
