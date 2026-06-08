#include "WindowEnumerator.h"
#include "WindowDescriptorBuilder.h"
#include "utils/Util.h"
#include <QDebug>
#include <QFileInfo>
#include <shlobj_core.h>

namespace WindowEnumerator {

    bool isWindowAcceptable(HWND hwnd, bool skipVisibleCheck) {
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
        static const bool isUserAdmin = IsUserAnAdmin();
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
