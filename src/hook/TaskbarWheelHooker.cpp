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
            auto element = UIAutomation::getElementUnderMouse();
            qDebug() << delta << element.getClassName() << element.getAutomationId() << element.getName();
            if (element.getClassName() == "CEF-OSC-WIDGET") {
                qDebug() << "detect CEF, walk up to find TaskListButton";
                element = UIAutomation::findAncestorByClassName(element, "Taskbar.TaskListButtonAutomationPeer");
                if (!element.isValid()) {
                    qDebug() << "no Taskbar button ancestor found, skip";
                    return CallNextHookEx(nullptr, nCode, wParam, lParam);
                }
            }
            if (element.getClassName() == "Taskbar.TaskListButtonAutomationPeer") {
                auto appid = element.getAutomationId().mid(QStringLiteral("Appid: ").size());
                auto name = element.getName();
                int windows = 0;
                const auto kWindowCountDelimiter = QStringLiteral(" - ");
                if (auto dashIdx = name.lastIndexOf(kWindowCountDelimiter); dashIdx != -1) {
                    std::stringstream ss(name.mid(dashIdx + kWindowCountDelimiter.size()).toStdString());
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
        static bool wasCursorOnTaskbar = false;
        HWND topLevelHwnd = Util::topWindowFromPoint(Util::getCursorPos());
        bool isTaskbar = Util::isTaskbarWindow(topLevelHwnd);
        if (wasCursorOnTaskbar != isTaskbar) {
            wasCursorOnTaskbar = isTaskbar;
            if (isTaskbar) {
                m_mouseHook = SetWindowsHookEx(WH_MOUSE_LL, (HOOKPROC) mouseProc, GetModuleHandle(nullptr), 0);
                if (m_mouseHook == nullptr)
                    qCritical() << "Failed to install m_mouseHook";
                else
                    qDebug() << "Enter Taskbar" << QTime::currentTime()
                             << "hwnd=" << Qt::hex << topLevelHwnd << Qt::dec
                             << "class=" << Util::getClassName(topLevelHwnd);
            } else {
                UnhookWindowsHookEx(m_mouseHook);
                m_mouseHook = nullptr;
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
    if (m_mouseHook) {
        UnhookWindowsHookEx(m_mouseHook);
        qDebug() << "MouseHooker uninstalled";
    }
    s_instance = nullptr;
    UIAutomation::cleanup();
}

void TaskbarWheelHooker::setPaused(bool paused) {
    m_paused = paused;
}
