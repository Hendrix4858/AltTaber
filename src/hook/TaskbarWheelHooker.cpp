#include "hook/TaskbarWheelHooker.h"
#include <QTimer>
#include "hook/uiautomation.h"
#include "utils/AppUtil.h"
#include "utils/Util.h"
#include <QTime>

namespace { TaskbarWheelHooker* s_instance = nullptr; }

LRESULT mouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && wParam == WM_MOUSEWHEEL) {
        if (!s_instance || s_instance->m_paused)
            return CallNextHookEx(nullptr, nCode, wParam, lParam);

        auto* data = (MSLLHOOKSTRUCT*) lParam;
        HWND topLevelHwnd = Util::topWindowFromPoint(data->pt);
        if (Util::isTaskbarWindow(topLevelHwnd)) {
            auto delta = (short) HIWORD(data->mouseData);
            qDebug() << "--- Taskbar Mouse Wheel" << (delta > 0 ? "↑" : "↓")
                     << "hwnd=" << Qt::hex << topLevelHwnd << Qt::dec
                     << "class=" << Util::getClassName(topLevelHwnd)
                     << "pt=(" << data->pt.x << "," << data->pt.y << ")";
            auto el = UIAutomation::getElementUnderMouse();
            qDebug() << delta << el.getClassName() << el.getAutomationId() << el.getName();
            if (el.getClassName() == "CEF-OSC-WIDGET") {
                qDebug() << "detect CEF, walk up to find TaskListButton";
                el = UIAutomation::findAncestorByClassName(el, "Taskbar.TaskListButtonAutomationPeer");
                if (!el.isValid()) {
                    qDebug() << "no Taskbar button ancestor found, skip";
                    return CallNextHookEx(nullptr, nCode, wParam, lParam);
                }
            }
            if (el.getClassName() == "Taskbar.TaskListButtonAutomationPeer") {
                auto appid = el.getAutomationId().mid(QStringLiteral("Appid: ").size());
                auto name = el.getName();
                int windows = 0;
                const auto Dash = QStringLiteral(" - ");
                if (auto dashIdx = name.lastIndexOf(Dash); dashIdx != -1) {
                    std::stringstream ss(name.mid(dashIdx + Dash.size()).toStdString());
                    ss >> windows;
                    name = name.left(dashIdx);
                }
                auto exePath = AppUtil::getExePathFromAppIdOrName(appid, name);
                if (s_instance)
                    emit s_instance->tabWheelEvent(exePath, delta > 0, windows);
            }
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

TaskbarWheelHooker::TaskbarWheelHooker() {
    if (s_instance) {
        qCritical() << "Only one TaskbarWheelHooker can be installed!";
        return;
    }
    s_instance = this;
    AppUtil::getExePathFromAppIdOrName(); // cache

    auto* timer = new QTimer(this);
    timer->callOnTimeout(this, [this]() {
        static bool isLastTaskbar = false;
        HWND topLevelHwnd = Util::topWindowFromPoint(Util::getCursorPos());
        bool isTaskbar = Util::isTaskbarWindow(topLevelHwnd);
        if (isLastTaskbar != isTaskbar) {
            isLastTaskbar = isTaskbar;
            if (isTaskbar) {
                h_mouse = SetWindowsHookEx(WH_MOUSE_LL, (HOOKPROC) mouseProc, GetModuleHandle(nullptr), 0);
                if (h_mouse == nullptr)
                    qCritical() << "Failed to install h_mouse";
                else
                    qDebug() << "Enter Taskbar" << QTime::currentTime()
                             << "hwnd=" << Qt::hex << topLevelHwnd << Qt::dec
                             << "class=" << Util::getClassName(topLevelHwnd);
            } else {
                UnhookWindowsHookEx(h_mouse);
                h_mouse = nullptr;
                auto pt = Util::getCursorPos();
                auto className = Util::getClassName(topLevelHwnd);
                qDebug() << "Leave Taskbar" << QTime::currentTime()
                         << "hwnd=" << Qt::hex << topLevelHwnd << Qt::dec
                         << "class=" << className
                         << "x=" << pt.x << "y=" << pt.y;
                emit leaveTaskbar();
            }
        }
    });
    timer->start(50);
}

TaskbarWheelHooker::~TaskbarWheelHooker() {
    if (h_mouse) {
        UnhookWindowsHookEx(h_mouse);
        qDebug() << "MouseHooker uninstalled";
    }
    s_instance = nullptr;
    UIAutomation::cleanup();
}

void TaskbarWheelHooker::setPaused(bool paused) {
    m_paused = paused;
}
