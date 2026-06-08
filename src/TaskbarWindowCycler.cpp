#include "TaskbarWindowCycler.h"
#include "GroupWindowCycler.h"
#include "WindowManager.h"
#include "utils/Util.h"
#include <QDebug>
#include <QSet>

TaskbarWindowCycler::TaskbarWindowCycler(GroupWindowCycler* cyc, WindowManager* wm, QObject* parent)
    : QObject(parent), m_cyc(cyc), m_wm(wm) {}

void TaskbarWindowCycler::rotate(const QString& exePath, bool forward, int windows) {
    qDebug() << "(Taskbar)Wheel on:" << exePath << forward << windows;
    if (exePath.isEmpty()) return;
    if (!windows) {
        qDebug() << "No window for this app";
        return;
    }

    if (lastTaskbarPath != exePath) {
        lastTaskbarPath = exePath;
        m_cyc->clearGroupWindowOrder();
    }
    auto& tbOrder = m_cyc->groupWindowOrder();
    if (tbOrder.isEmpty()) {
        tbOrder = m_wm->filteredHwndsForExe(exePath);
        lastTaskbarHwnd = nullptr;
    }

    if (tbOrder.isEmpty()) {
        qWarning() << "No window in group" << exePath;
        auto childPaths = Util::getChildProcessPaths(exePath);
        if (childPaths.isEmpty()) return;
        if (childPaths.size() == 1) {
            qDebug() << "Try to switch to child process:" << childPaths.first();
            tbOrder = m_wm->filteredHwndsForExe(childPaths.first());
        } else {
            qWarning() << "Multiple child processes" << childPaths;
            QSet<QString> validPaths;
            for (auto hwnd : Util::listValidWindows()) {
                if (auto path = Util::getWindowProcessPath(hwnd); !path.isEmpty())
                    validPaths.insert(path.toLower());
            }
            for (auto& path : childPaths) {
                if (validPaths.contains(path.toLower())) {
                    qDebug() << "Try to switch to valid child process:" << path;
                    tbOrder = m_wm->filteredHwndsForExe(path);
                    break;
                }
            }
        }
        if (tbOrder.isEmpty())
            return;
    }

    HWND hwnd = nullptr;
    if (!lastTaskbarHwnd) {
        hwnd = tbOrder.first();
        if (forward && hwnd == GetForegroundWindow())
            hwnd = GroupWindowCycler::rotateWindowInGroup(tbOrder, hwnd, true);
    } else {
        if (lastTaskbarForward == forward)
            hwnd = GroupWindowCycler::rotateWindowInGroup(tbOrder, lastTaskbarHwnd, forward);
        else
            hwnd = lastTaskbarHwnd;
    }
    lastTaskbarForward = forward;

    static auto mouseEvent = [](DWORD flag) {
        mouse_event(flag, 0, 0, 0, 0);
    };
    if (forward) {
        if (windows == 1) {
            if ((hwnd != GetForegroundWindow() || IsIconic(hwnd))) {
                mouseEvent(MOUSEEVENTF_LEFTDOWN);
                mouseEvent(MOUSEEVENTF_LEFTUP);
                qDebug() << "(Taskbar)Switch by click";
            }
        } else {
            if (HWND thumbnail = Util::getCurrentTaskListThumbnailWnd(); IsWindowVisible(thumbnail)) {
                qDebug() << "(Taskbar)Press LButton";
                mouseEvent(MOUSEEVENTF_LEFTDOWN);
                QTimer::singleShot(20, this, [hwnd]() {
                    Util::switchToWindow(hwnd, true);
                });
            } else
                Util::switchToWindow(hwnd, true);

            if (!taskbarTimer) {
                taskbarTimer = new QTimer(this);
                taskbarTimer->setSingleShot(true);
                taskbarTimer->setInterval(200);
                connect(taskbarTimer, &QTimer::timeout, this, [this]() {
                    mouseEvent(MOUSEEVENTF_LEFTUP);
                    qDebug() << "(Taskbar)Release LButton";
                    QTimer::singleShot(100, this, []() {
                        if (HWND thumbnail = Util::getCurrentTaskListThumbnailWnd(); IsWindowVisible(thumbnail)) {
                            if (HWND taskbar = FindWindow(L"Shell_TrayWnd", nullptr))
                                Util::switchToWindow(taskbar, true);
                        }
                    });
                });
            }
            taskbarTimer->stop();
            taskbarTimer->start();
        }
        qDebug() << "(Taskbar)Switch to" << hwnd << Util::getWindowTitle(hwnd) << Util::getClassName(hwnd);
    } else {
        if (auto normal = GroupWindowCycler::rotateNormalWindowInGroup(tbOrder, hwnd, false)) {
            if (normal != hwnd)
                qDebug() << "(Taskbar)Skip minimized" << hwnd << "->" << normal;
            hwnd = normal;
            ShowWindow(hwnd, SW_MINIMIZE);
            qDebug() << "(Taskbar)Minimize" << hwnd << Util::getWindowTitle(hwnd) << Util::getClassName(hwnd);
        }
    }

    lastTaskbarHwnd = hwnd;
}

void TaskbarWindowCycler::clearOrder() {
    m_cyc->clearGroupWindowOrder();
}
