#include "utils/ProcessUtil.h"
#include <QDebug>
#include <psapi.h>
#include <qoperatingsystemversion.h>
#include <tlhelp32.h>
#include <QApplication>
#include "utils/AppUtil.h"
#include "utils/MiscUtil.h"
#include "utils/WindowUtil.h"

namespace Util {
    bool isProcessElevated(HANDLE hProcess) {
        HANDLE hToken = nullptr;
        if (!OpenProcessToken(hProcess, TOKEN_QUERY, &hToken)) {
            qWarning() << "OpenProcessToken failed" << GetLastError();
            return false;
        }

        TOKEN_ELEVATION elevation;
        DWORD size;
        bool isElevated = false;

        if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &size)) {
            isElevated = (elevation.TokenIsElevated != 0);
        } else {
            qWarning() << "GetTokenInformation failed" << GetLastError();
        }

        CloseHandle(hToken);
        return isElevated;
    }

    bool isWindowElevated(HWND hwnd) {
        DWORD pid;
        GetWindowThreadProcessId(hwnd, &pid);

        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (hProcess == nullptr) {
            qWarning() << "OpenProcess failed" << GetLastError();
            return false;
        }

        bool isAdmin = isProcessElevated(hProcess);

        CloseHandle(hProcess);
        return isAdmin;
    }

    QString getProcessPath(DWORD pid) {
        QString path;
        static const bool isWin10orHigher = (QOperatingSystemVersion::current() >= QOperatingSystemVersion::Windows10);
        HANDLE hProcess = nullptr;
        if (isWin10orHigher)
            hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        else
            hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);

        if (hProcess) {
            TCHAR processName[MAX_PATH];
            if (GetModuleFileNameEx(hProcess, nullptr, processName, MAX_PATH))
                path = QString::fromWCharArray(processName);
            CloseHandle(hProcess);
        }
        return path;
    }

    QString getWindowProcessPath(HWND hwnd) {
        auto originalHwnd = hwnd;
        HWND coreHwnd = nullptr;
        DWORD pid = 0;

        if (AppUtil::isAppFrameWindow(hwnd))
            coreHwnd = AppUtil::getAppCoreWindow(hwnd);

        // Strategy 1: CoreWindow found — use its PID directly
        if (coreHwnd) {
            GetWindowThreadProcessId(coreHwnd, &pid);
            if (pid) return getProcessPath(pid);
        }

        // Strategy 2: Enumerate child windows to find one from a different process
        if (AppUtil::isAppFrameWindow(originalHwnd)) {
            DWORD framePid = 0;
            GetWindowThreadProcessId(originalHwnd, &framePid);
            for (auto child : Util::enumChildWindows(originalHwnd)) {
                DWORD childPid = 0;
                GetWindowThreadProcessId(child, &childPid);
                if (childPid != 0 && childPid != framePid) {
                    auto childPath = getProcessPath(childPid);
                    if (!childPath.isEmpty()) return childPath;
                }
            }
        }

        // Strategy 3: Fallback to original window's PID
        GetWindowThreadProcessId(originalHwnd, &pid);
        if (pid) return getProcessPath(pid);

        return {};
    }

    DWORD findProcessByPath(const QString& exePath) {
        DWORD targetPID = 0;
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE) {
            return 0;
        }

        PROCESSENTRY32 entry = {sizeof(PROCESSENTRY32)};
        if (Process32First(snapshot, &entry)) {
            do {
                QString processPath = getProcessPath(entry.th32ProcessID);
                if (QString::compare(processPath, exePath, Qt::CaseInsensitive) == 0) {
                    targetPID = entry.th32ProcessID;
                    break;
                }
            } while (Process32Next(snapshot, &entry));
        }

        CloseHandle(snapshot);
        return targetPID;
    }

    QList<QString> getChildProcessPaths(DWORD parentPID) {
        QList<QString> childPaths;

        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE) {
            return childPaths;
        }

        PROCESSENTRY32 entry = {sizeof(PROCESSENTRY32)};
        if (Process32First(snapshot, &entry)) {
            do {
                if (entry.th32ParentProcessID == parentPID) {
                    QString processPath = getProcessPath(entry.th32ProcessID);
                    if (!processPath.isEmpty()) {
                        childPaths.append(processPath);
                    }
                }
            } while (Process32Next(snapshot, &entry));
        }

        CloseHandle(snapshot);
        return childPaths;
    }

    QList<QString> getChildProcessPaths(const QString& exePath) {
        if (auto pid = findProcessByPath(exePath)) {
            return getChildProcessPaths(pid);
        }
        return {};
    }

    void switchToWindow(HWND hwnd, bool force) {
        bool iconic = IsIconic(hwnd);
        if (iconic)
            ShowWindow(hwnd, SW_RESTORE);
        if (force) {
            qInfo().noquote() << "[switchToWindow]" << "target=" << Qt::hex << hwnd << Qt::dec
                     << "force=1" << "iconic=" << iconic
                     << "caller_fg=" << Qt::hex << GetForegroundWindow() << Qt::dec
                     << "path=SendInput+SetForegroundWindow";
            INPUT input = {.type = INPUT_KEYBOARD};
            SendInput(1, &input, sizeof(INPUT));
            SetForegroundWindow(hwnd);
        } else {
            qInfo().noquote() << "[switchToWindow]" << "target=" << Qt::hex << hwnd << Qt::dec
                     << "force=0" << "iconic=" << iconic
                     << "caller_fg=" << Qt::hex << GetForegroundWindow() << Qt::dec
                     << "path=SetForegroundWindow";
            SetForegroundWindow(hwnd);
        }
    }

    void bringWindowToTop(HWND hwnd, HWND hWndInsertAfter) {
        if (isTopMost(hwnd)) return;
        LockSetForegroundWindow(LSFW_LOCK);
        if (IsIconic(hwnd))
            ShowWindow(hwnd, SW_SHOWNOACTIVATE);
        SetWindowPos(hwnd, hWndInsertAfter, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        LockSetForegroundWindow(LSFW_UNLOCK);
    }
}
