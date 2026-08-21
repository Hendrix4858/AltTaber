#include <QTimer>
#include <QRegularExpression>
#include "hook/TaskbarWheelHooker.h"
#include "hook/uiautomation.h"
#include "utils/AppUtil.h"
#include "utils/Util.h"

namespace { TaskbarWheelHooker* s_instance = nullptr; }

LRESULT mouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode != HC_ACTION)
        return CallNextHookEx(nullptr, nCode, wParam, lParam);

    auto* data = (MSLLHOOKSTRUCT*) lParam;
    HWND topLevelHwnd = Util::topWindowFromPoint(data->pt);
    bool isTaskbar = Util::isTaskbarWindow(topLevelHwnd);

    if (wParam == WM_MOUSEMOVE) {
        if (s_instance && s_instance->m_onTaskbar != isTaskbar) {
            s_instance->m_onTaskbar = isTaskbar;
            if (!isTaskbar) {
                s_instance->m_accumulatedDelta = 0;
                s_instance->m_debounceTimer->stop();
                emit s_instance->leaveTaskbar();
            }
        }
        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }

    if (wParam == WM_MOUSEWHEEL) {
        if (s_instance && !s_instance->m_paused && isTaskbar) {
            auto delta = (short) HIWORD(data->mouseData);
            s_instance->m_accumulatedDelta += delta;
            s_instance->m_debounceTimer->start(80);
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

    m_debounceTimer = new QTimer(this);
    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(80);
    connect(m_debounceTimer, &QTimer::timeout, this, &TaskbarWheelHooker::onDebounceTimeout);

    m_mouseHook = SetWindowsHookEx(WH_MOUSE_LL, (HOOKPROC) mouseProc, GetModuleHandle(nullptr), 0);
    if (m_mouseHook == nullptr)
        qCritical() << "Failed to install WH_MOUSE_LL, error:" << GetLastError();
}

TaskbarWheelHooker::~TaskbarWheelHooker() {
    if (m_mouseHook) {
        UnhookWindowsHookEx(m_mouseHook);
    }
    s_instance = nullptr;
    UIAutomation::cleanup();
}

void TaskbarWheelHooker::setPaused(bool paused) {
    m_paused = paused;
}

void TaskbarWheelHooker::onDebounceTimeout() {
    if (!s_instance || m_paused) return;

    bool forward = m_accumulatedDelta > 0;
    m_accumulatedDelta = 0;

    auto element = UIAutomation::getElementUnderMouse();
    if (!element.isValid()) return;

    if (element.getClassName() == "CEF-OSC-WIDGET") {
        element = UIAutomation::findAncestorByClassName(element, "Taskbar.TaskListButtonAutomationPeer");
        if (!element.isValid()) return;
    }
    if (element.getClassName() == "Taskbar.TaskListButtonAutomationPeer") {
        auto appid = element.getAutomationId().mid(QStringLiteral("Appid: ").size());
        auto name = element.getName();
        int windows = 0;
        const auto kWindowCountDelimiter = QStringLiteral(" - ");
        if (auto dashIdx = name.lastIndexOf(kWindowCountDelimiter); dashIdx != -1) {
            static const QRegularExpression leadingNum(R"(^(\d+))");
            auto countStr = name.mid(dashIdx + kWindowCountDelimiter.size());
            auto match = leadingNum.match(countStr);
            if (match.hasMatch())
                windows = match.captured(1).toInt();
            name = name.left(dashIdx);
        }
        auto exePath = AppUtil::getExePathFromAppIdOrName(appid, name);
        if (!exePath.isEmpty())
            emit tabWheelEvent(exePath, forward, windows, appid);
    }
}
