#include "TaskbarWindowCycler.h"
#include "TaskbarMouseHelper.h"
#include "GroupWindowCycler.h"
#include "WindowManager.h"
#include "core/ConfigManager.h"
#include "utils/Util.h"
#include "utils/PwaDetector.h"
#include <QDebug>
#include <QSet>

TaskbarWindowCycler::TaskbarWindowCycler(GroupWindowCycler* cyc, WindowManager* wm, QObject* parent)
    : QObject(parent), m_groupCycler(cyc), m_windowManager(wm) {}

void TaskbarWindowCycler::rotate(const QString& exePath, bool forward, int windowCount, const QString& appid) {
    if (!ConfigManager::instance().getTaskbarWheelEnabled()) return;
    if (exePath.isEmpty()) return;
    if (!windowCount) return;

    qWarning() << "[TaskbarWheel] rotate: exe=" << exePath
               << "fwd=" << forward << "cnt=" << windowCount
               << "appid=" << appid;

    if (m_lastTaskbarExePath != exePath) {
        m_lastTaskbarExePath = exePath;
        m_lastTaskbarAppid = appid;
        m_groupCycler->clearGroupWindowOrder();
    }
    auto& tbOrder = m_groupCycler->groupWindowOrder();
    if (tbOrder.isEmpty()) {
        tbOrder = m_windowManager->filteredHwndsForExe(exePath);
        m_lastTaskbarHwnd = nullptr;
    }

    if (tbOrder.isEmpty()) {
        qWarning() << "[TaskbarWheel] no window by exePath:" << exePath;
        auto childPaths = Util::getChildProcessPaths(exePath);
        if (childPaths.isEmpty()) {
            if (!appid.isEmpty()) {
                for (auto hwnd : Util::listValidWindows()) {
                    auto aumid = PwaDetector::getAppUserModelId(hwnd);
                    if (!aumid.isEmpty() && aumid == appid)
                        tbOrder.append(hwnd);
                }
                qWarning() << "[TaskbarWheel] AUMID fallback: found" << tbOrder.size() << "windows";
            }
            if (tbOrder.isEmpty()) return;
        } else if (childPaths.size() == 1) {
            tbOrder = m_windowManager->filteredHwndsForExe(childPaths.first());
        } else {
            qWarning() << "[TaskbarWheel] multiple child processes" << childPaths;
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
        if (tbOrder.isEmpty()) return;
    }

    if (windowCount == 1 && tbOrder.size() > 1) {
        qWarning() << "[TaskbarWheel] clipping tbOrder from" << tbOrder.size() << "to 1 (windowCount=1)";
        tbOrder = {tbOrder.first()};
        m_lastTaskbarHwnd = nullptr;
    }

    HWND hwnd = nullptr;
    if (!m_lastTaskbarHwnd) {
        hwnd = tbOrder.first();
        qWarning() << "[TaskbarWheel] first hwnd=" << Qt::hex << hwnd << Qt::dec
                   << "fg=" << Qt::hex << GetForegroundWindow() << Qt::dec
                   << "iconic=" << IsIconic(hwnd);
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
            bool doClick = (hwnd != GetForegroundWindow() || IsIconic(hwnd));
            qWarning() << "[TaskbarWheel] fwd-single: hwnd=" << Qt::hex << hwnd << Qt::dec
                       << "isFg=" << (hwnd == GetForegroundWindow())
                       << "iconic=" << IsIconic(hwnd) << "doClick=" << doClick;
            if (doClick) {
                TaskbarMouseHelper::click();
                qWarning() << "[TaskbarWheel] click done";
            }
        } else {
            qWarning() << "[TaskbarWheel] fwd-multi: hwnd=" << Qt::hex << hwnd << Qt::dec;
            HWND thumbnail = Util::getCurrentTaskListThumbnailWnd();
            if (thumbnail && IsWindowVisible(thumbnail)) {
                qWarning() << "[TaskbarWheel] thumbnail visible, hold+switch";
                TaskbarMouseHelper::hold();
                QTimer::singleShot(20, this, [hwnd]() {
                    Util::switchToWindow(hwnd, true);
                });
            } else {
                qWarning() << "[TaskbarWheel] no thumbnail, direct switch";
                Util::switchToWindow(hwnd, true);
            }

            if (!m_releaseTimer) {
                m_releaseTimer = new QTimer(this);
                m_releaseTimer->setSingleShot(true);
                m_releaseTimer->setInterval(200);
                connect(m_releaseTimer, &QTimer::timeout, this, [this]() {
                    TaskbarMouseHelper::release();
                    QTimer::singleShot(100, this, []() {
                        HWND thumbnail = Util::getCurrentTaskListThumbnailWnd();
                        if (thumbnail && IsWindowVisible(thumbnail)) {
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
        qWarning() << "[TaskbarWheel] backward: hwnd=" << Qt::hex << hwnd << Qt::dec
                   << "iconic=" << IsIconic(hwnd);
        if (auto normal = GroupWindowCycler::rotateToNormalWindow(tbOrder, hwnd, false)) {
            qWarning() << "[TaskbarWheel] backward: normal=" << Qt::hex << normal << Qt::dec;
            hwnd = normal;
            SendMessage(hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
        } else {
            qWarning() << "[TaskbarWheel] backward: no normal window";
        }
    }

    m_lastTaskbarHwnd = hwnd;
}

void TaskbarWindowCycler::clearOrder() {
    m_groupCycler->clearGroupWindowOrder();
}
