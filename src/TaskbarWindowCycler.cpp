#include "TaskbarWindowCycler.h"
#include "TaskbarMouseHelper.h"
#include "GroupWindowCycler.h"
#include "WindowManager.h"
#include "utils/Util.h"
#include <QDebug>
#include <QSet>

TaskbarWindowCycler::TaskbarWindowCycler(GroupWindowCycler* cyc, WindowManager* wm, QObject* parent)
    : QObject(parent), m_groupCycler(cyc), m_windowManager(wm) {}

void TaskbarWindowCycler::rotate(const QString& exePath, bool forward, int windowCount) {
    if (exePath.isEmpty()) return;
    if (!windowCount) {
        return;
    }

    if (m_lastTaskbarExePath != exePath) {
        m_lastTaskbarExePath = exePath;
        m_groupCycler->clearGroupWindowOrder();
    }
    auto& tbOrder = m_groupCycler->groupWindowOrder();
    if (tbOrder.isEmpty()) {
        tbOrder = m_windowManager->filteredHwndsForExe(exePath);
        m_lastTaskbarHwnd = nullptr;
    }

    if (tbOrder.isEmpty()) {
        qWarning() << "No window in group" << exePath;
        auto childPaths = Util::getChildProcessPaths(exePath);
        if (childPaths.isEmpty()) return;
        if (childPaths.size() == 1) {
            tbOrder = m_windowManager->filteredHwndsForExe(childPaths.first());
        } else {
            qWarning() << "Multiple child processes" << childPaths;
            QSet<QString> validPaths;
            for (auto hwnd : Util::listValidWindows()) {
                if (auto path = Util::getWindowProcessPath(hwnd); !path.isEmpty())
                    validPaths.insert(path.toLower());
            }
            for (auto& path : childPaths) {
                if (validPaths.contains(path.toLower())) {
                    tbOrder = m_windowManager->filteredHwndsForExe(path);
                    break;
                }
            }
        }
        if (tbOrder.isEmpty())
            return;
    }

    HWND hwnd = nullptr;
    if (!m_lastTaskbarHwnd) {
        hwnd = tbOrder.first();
        if (forward && hwnd == GetForegroundWindow())
            hwnd = GroupWindowCycler::rotateWindow(tbOrder, hwnd, true);
    } else {
        if (m_lastTaskbarDirection == forward)
            hwnd = GroupWindowCycler::rotateWindow(tbOrder, m_lastTaskbarHwnd, forward);
        else
            hwnd = m_lastTaskbarHwnd;
    }
    m_lastTaskbarDirection = forward;

    if (forward) {
        if (windowCount == 1) {
            if ((hwnd != GetForegroundWindow() || IsIconic(hwnd))) {
                TaskbarMouseHelper::click();
            }
        } else {
            if (HWND thumbnail = Util::getCurrentTaskListThumbnailWnd(); IsWindowVisible(thumbnail)) {
                TaskbarMouseHelper::hold();
                QTimer::singleShot(20, this, [hwnd]() {
                    Util::switchToWindow(hwnd, true);
                });
            } else
                Util::switchToWindow(hwnd, true);

            if (!m_releaseTimer) {
                m_releaseTimer = new QTimer(this);
                m_releaseTimer->setSingleShot(true);
                m_releaseTimer->setInterval(200);
                connect(m_releaseTimer, &QTimer::timeout, this, [this]() {
                    TaskbarMouseHelper::release();
                    QTimer::singleShot(100, this, []() {
                        if (HWND thumbnail = Util::getCurrentTaskListThumbnailWnd(); IsWindowVisible(thumbnail)) {
                            if (HWND taskbar = FindWindow(L"Shell_TrayWnd", nullptr))
                                Util::switchToWindow(taskbar, true);
                        }
                    });
                });
            }
            m_releaseTimer->stop();
            m_releaseTimer->start();
        }
    } else {
        if (auto normal = GroupWindowCycler::rotateToNormalWindow(tbOrder, hwnd, false)) {
            hwnd = normal;
            ShowWindow(hwnd, SW_MINIMIZE);
        }
    }

    m_lastTaskbarHwnd = hwnd;
}

void TaskbarWindowCycler::clearOrder() {
    m_groupCycler->clearGroupWindowOrder();
}
