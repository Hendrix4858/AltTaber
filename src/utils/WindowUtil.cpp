#include "utils/WindowUtil.h"
#include <QDebug>
#include <QFileInfo>
#include <QHash>
#include <vector>
#include <winternl.h>
#include <ExDisp.h>
#include <ShlGuid.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <shobjidl.h>
#include <KnownFolders.h>
#include "utils/MiscUtil.h"
#include "utils/ProcessUtil.h"
#include "utils/AppUtil.h"
#include "utils/PwaDetector.h"
#include "core/ConfigManager.h"
#include "WindowFilter.h"
#include "WindowDescriptorBuilder.h"

namespace Util {

    bool isWindowAllowed(HWND hwnd, bool skipVisibleCheck) {
        // Structural check (requires HWND)
        if (!WindowEnumerator::isWindowAcceptable(hwnd, skipVisibleCheck))
            return false;
        // System + user filter via WindowFilter
        WindowFilter filter;
        filter.setRules(WindowFilter::buildRuleFromConfig(cfg()));
        WindowDescriptor desc = WindowDescriptorBuilder::fromHwnd(hwnd);
        return filter.isAllowed(desc);
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

    void closeXamlWindowedPopup() {
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
        closeXamlWindowedPopup();
    }

    QList<HWND> listValidWindows() {
        auto descriptors = WindowEnumerator::enumValidWindows();
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

    namespace {
        struct IdentityCacheEntry {
            DWORD processId = 0;
            AppIdentity identity;
        };

        QHash<HWND, IdentityCacheEntry>& identityCache() {
            static QHash<HWND, IdentityCacheEntry> cache;
            return cache;
        }



        QString extractMscName(HWND hwnd) {
            DWORD pid = 0;
            GetWindowThreadProcessId(hwnd, &pid);

            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                                          FALSE, pid);
            if (!hProcess) return {};

            typedef NTSTATUS(NTAPI *NtQueryInfoProc_t)(HANDLE, PROCESSINFOCLASS,
                                                        PVOID, ULONG, PULONG);
            auto NtQueryInformationProcess = (NtQueryInfoProc_t)
                GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueryInformationProcess");
            if (!NtQueryInformationProcess) { CloseHandle(hProcess); return {}; }

            PROCESS_BASIC_INFORMATION pbi;
            ULONG retLen = 0;
            NTSTATUS status = NtQueryInformationProcess(hProcess, ProcessBasicInformation,
                                                        &pbi, sizeof(pbi), &retLen);
            if (status < 0 || !pbi.PebBaseAddress) { CloseHandle(hProcess); return {}; }

            PEB peb;
            SIZE_T bytesRead = 0;
            if (!ReadProcessMemory(hProcess, pbi.PebBaseAddress, &peb,
                                   sizeof(peb), &bytesRead)) {
                CloseHandle(hProcess); return {};
            }

            RTL_USER_PROCESS_PARAMETERS params;
            if (!ReadProcessMemory(hProcess, peb.ProcessParameters, &params,
                                   sizeof(params), &bytesRead)) {
                CloseHandle(hProcess); return {};
            }

            ULONG bufChars = params.CommandLine.Length / sizeof(wchar_t);
            std::vector<wchar_t> cmdBuf(bufChars + 1, L'\0');
            if (!ReadProcessMemory(hProcess, params.CommandLine.Buffer,
                                   cmdBuf.data(), params.CommandLine.Length,
                                   &bytesRead)) {
                CloseHandle(hProcess); return {};
            }
            CloseHandle(hProcess);

            QString cmdLine = QString::fromWCharArray(cmdBuf.data());
            bool inQuote = false;
            QStringList args;
            QString cur;
            for (int i = 0; i < cmdLine.size(); ++i) {
                QChar c = cmdLine[i];
                if (c == L'"') { inQuote = !inQuote; continue; }
                if (c == L' ' && !inQuote) {
                    if (!cur.isEmpty()) { args.append(cur); cur.clear(); }
                    continue;
                }
                cur += c;
            }
            if (!cur.isEmpty()) args.append(cur);

            for (const auto& arg : args) {
                if (arg.endsWith(QStringLiteral(".msc"), Qt::CaseInsensitive))
                    return arg; // full path for iconKey
            }

            return {};
        }
    }

    AppIdentity Util::resolveIdentity(HWND hwnd, const QString& knownAumid) {
        auto& cache = identityCache();

        DWORD currentPid = 0;
        GetWindowThreadProcessId(hwnd, &currentPid);

        auto it = cache.constFind(hwnd);
        if (it != cache.constEnd() && it->processId == currentPid)
            return it->identity;

        // Layer 1: AUMID (stable app identity)
        if (knownAumid == QLatin1StringView("Microsoft.Windows.ControlPanel")) {
            AppIdentity id;
            id.host = QStringLiteral("explorer.exe");
            id.instance = QStringLiteral("ControlPanel");
            id.appUserModelId = knownAumid;
            cache[hwnd] = {currentPid, id};
            return id;
        }

        wchar_t className[256];
        if (!GetClassNameW(hwnd, className, 256)) {
            AppIdentity id;
            cache[hwnd] = {currentPid, id};
            return id;
        }

        QString processPath = Util::getWindowProcessPath(hwnd);
        QString processName = QFileInfo(processPath).fileName().toLower();

        // Layer 2: MMC snap-in identity
        if (processName == QStringLiteral("mmc.exe")
            && wcscmp(className, L"MMCMainFrame") == 0) {
            QString mscPath = extractMscName(hwnd);
            AppIdentity id;
            id.host = processPath;
            id.instance = mscPath; // full path for icon resolution
            cache[hwnd] = {currentPid, id};
            return id;
        }

        // Layer 3: Explorer folder identity — all folder windows share one group
        if (processName == QStringLiteral("explorer.exe")
            && wcscmp(className, L"CabinetWClass") == 0) {
            AppIdentity id;
            id.host = processPath;
            cache[hwnd] = {currentPid, id};
            return id;
        }

        // Layer 4: PWA — use AUMID as instance for unique groupKey()
        QString aumid = knownAumid;
        if (aumid.isEmpty() && PwaDetector::mayHostPwa(processPath))
            aumid = PwaDetector::getAppUserModelId(hwnd);
        if (!aumid.isEmpty() && PwaDetector::isPwaWindow(processPath, aumid)) {
            AppIdentity id;
            id.host = processPath;
            id.instance = aumid;
            id.appUserModelId = aumid;
            cache[hwnd] = {currentPid, id};
            return id;
        }

        // Layer 5: Default — use process path as identity
        AppIdentity id;
        id.host = processPath;
        cache[hwnd] = {currentPid, id};
        return id;
    }

    void Util::clearIdentityCache() {
        identityCache().clear();
    }
}
