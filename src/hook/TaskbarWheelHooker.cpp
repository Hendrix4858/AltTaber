#include "hook/TaskbarWheelHooker.h"
#include <QTimer>
#include "hook/uiautomation.h"
#include "utils/AppUtil.h"
#include "utils/Util.h"

namespace { TaskbarWheelHooker* s_instance = nullptr; }

LRESULT mouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && wParam == WM_MOUSEWHEEL) {
        if (!s_instance || s_instance->m_paused)
            return CallNextHookEx(nullptr, nCode, wParam, lParam);

        auto* data = (MSLLHOOKSTRUCT*) lParam;
        HWND topLevelHwnd = Util::topWindowFromPoint(data->pt);
        if (Util::isTaskbarWindow(topLevelHwnd)) {
            auto delta = (short) HIWORD(data->mouseData);
            auto element = UIAutomation::getElementUnderMouse();
            if (element.getClassName() == "CEF-OSC-WIDGET") {
                element = UIAutomation::findAncestorByClassName(element, "Taskbar.TaskListButtonAutomationPeer");
                if (!element.isValid()) {
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
            } else {
                UnhookWindowsHookEx(m_mouseHook);
                m_mouseHook = nullptr;
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
