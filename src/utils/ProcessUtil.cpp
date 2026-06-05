#include "utils/ProcessUtil.h"
#include <QDebug>
#include <psapi.h>
#include <qoperatingsystemversion.h>
#include <tlhelp32.h>
#include <QApplication>
#include "utils/AppUtil.h"
#include "utils/MiscUtil.h"

namespace Util {
    bool isProcessElevated(HANDLE hProcess) {
        HANDLE hToken = nullptr;
        if (!OpenProcessToken(hProcess, TOKEN_QUERY, &hToken)) {
            qDebug() << "OpenProcessToken failed" << GetLastError();
            return false;
        }

        TOKEN_ELEVATION elevation;
        DWORD size;
        bool isElevated = false;

        if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &size)) {
            isElevated = (elevation.TokenIsElevated != 0);
        } else {
            qDebug() << "GetTokenInformation failed" << GetLastError();
        }

        CloseHandle(hToken);
        return isElevated;
    }

    bool isWindowElevated(HWND hwnd) {
        DWORD pid;
        GetWindowThreadProcessId(hwnd, &pid);

        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (hProcess == nullptr) {
            qDebug() << "OpenProcess failed" << GetLastError();
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
        if (AppUtil::isAppFrameWindow(hwnd))
            hwnd = AppUtil::getAppCoreWindow(hwnd);

        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid)
            return getProcessPath(pid);
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
        if (IsIconic(hwnd))
            ShowWindow(hwnd, SW_RESTORE);
        if (force) {
            INPUT input = {.type = INPUT_KEYBOARD};
            SendInput(1, &input, sizeof(INPUT));
            SetForegroundWindow(hwnd);
        } else {
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
