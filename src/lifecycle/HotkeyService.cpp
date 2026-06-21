#include "lifecycle/HotkeyService.h"
#include "widget.h"
#include "TaskbarWindowCycler.h"
#include "hook/winEventHook.h"
#include "hook/TaskbarWheelHooker.h"
#include "hook/KeyboardHooker.h"
#include "core/ConfigManager.h"
#include "utils/Util.h"
#include "OverlayController.h"

#include <QDebug>
#include <QTimer>

HotkeyService::HotkeyService(ConfigManager* config, QObject* parent)
    : QObject(parent), m_config(config) {}

HotkeyService::~HotkeyService() {
    unhookWinEvent();
}

void HotkeyService::init(Widget* widget, const HotkeyBindings& bindings) {
    m_widget = widget;
    m_keyboardHooker = new KeyboardHooker((HWND) widget->winId());
    m_keyboardHooker->updateBindings(bindings);
    m_keyboardHooker->setPaused(m_config->getPaused());
    widget->updateOverlayBindings(bindings);

    m_taskbarHooker = new TaskbarWheelHooker;
    m_taskbarHooker->setPaused(m_config->getPaused());

    m_retryTimer = new QTimer(this);
    m_retryTimer->setInterval(kRetryIntervalMs);
    connect(m_retryTimer, &QTimer::timeout, this, &HotkeyService::retryFallbackShow);

    qInfo() << "[Main] Installing WinEvent hook for EVENT_SYSTEM_FOREGROUND";
    setWinEventHook([widget, this](DWORD event, HWND hwnd) {
        if (event == EVENT_SYSTEM_FOREGROUND) {
            widget->notifyForegroundChanged(hwnd);

            auto oState = widget->overlayController()->overlayState();

            // Visible + foreground changed + Alt not held → dismiss (click outside)
            if (oState == OverlayController::OverlayState::Visible
                && hwnd != (HWND)widget->winId()
                && !Util::isKeyPressed(VK_MENU)) {
                qInfo() << "[WinEvent] hiding overlay (clicked outside while Visible)";
                widget->hideOverlay();
                return;
            }

            if (oState != OverlayController::OverlayState::Hidden
                && hwnd != (HWND)widget->winId()
                && !Util::isKeyPressed(VK_MENU)
                && oState != OverlayController::OverlayState::Visible) {
                qInfo() << "[WinEvent] hiding overlay (fg changed while not Visible)";
                widget->hideOverlay();
            }

            auto className = Util::getClassName(hwnd);
            if (hwnd == GetForegroundWindow() && Util::isKeyPressed(VK_MENU) &&
                (className == "ForegroundStaging") &&
                widget->overlayController()->overlayState()
                    != OverlayController::OverlayState::Visible) {
                qInfo() << "[WinEvent] Task switcher detected (fallback)" << className;
                widget->requestShow(OverlayIntent::FallbackShow);
                // UIPI fallback: hook couldn't block Tab from elevated window.
                // Refresh modifier tracking from physical key state so overlay
                // auto-dismisses when user releases Alt.
                if (Util::isKeyPressed(VK_MENU))
                    m_keyboardHooker->activateTrackingFromPhysicalState();
                // Start retry timer to ensure overlay actually takes foreground
                m_retryCount = 0;
                m_retryTimer->start();
            }
        }
    });
}

void HotkeyService::wireSignals(Widget* widget) {
    QObject::connect(m_keyboardHooker, &KeyboardHooker::hotkeyTriggered,
                     widget, &Widget::handleGlobalAction, Qt::QueuedConnection);

    QObject::connect(m_keyboardHooker, &KeyboardHooker::overlayKeyTriggered,
                     widget, &Widget::handleHookOverlayAction, Qt::QueuedConnection);

    QObject::connect(m_keyboardHooker, &KeyboardHooker::activationModifiersReleased,
                     widget, &Widget::onActivationModifiersReleased, Qt::QueuedConnection);

    QObject::connect(widget, &Widget::overlayDismissed,
                     m_keyboardHooker, &KeyboardHooker::resetActivationModifiers);

    QObject::connect(widget, &Widget::overlayShown,
                     m_keyboardHooker, &KeyboardHooker::notifyOverlayShown);

    QObject::connect(m_taskbarHooker, &TaskbarWheelHooker::tabWheelEvent,
                     widget->taskbarCycler(), &TaskbarWindowCycler::rotate, Qt::QueuedConnection);

    QObject::connect(m_taskbarHooker, &TaskbarWheelHooker::leaveTaskbar,
                     widget->taskbarCycler(), &TaskbarWindowCycler::clearOrder, Qt::QueuedConnection);

    qInfo() << "[Main] Hotkey system initialized";
}

void HotkeyService::retryFallbackShow() {
    auto* ctrl = m_widget->overlayController();
    auto state = ctrl->overlayState();
    if (state != OverlayController::OverlayState::Visible || ctrl->isForeground()) {
        m_retryTimer->stop();
        m_retryCount = 0;
        return;
    }
    m_retryCount++;
    if (m_retryCount >= kMaxRetries) {
        qWarning() << "[WinEvent] Fallback retry exhausted, overlay may not be foreground";
        m_retryTimer->stop();
        m_retryCount = 0;
        return;
    }
    qInfo() << "[WinEvent] Fallback retry" << m_retryCount;
    m_widget->requestShow(OverlayIntent::FallbackShow);
    if (Util::isKeyPressed(VK_MENU))
        m_keyboardHooker->activateTrackingFromPhysicalState();
}

void HotkeyService::reloadFromConfig() {
    qInfo() << "[Config] configEdited -> re-injecting hotkey bindings";
    auto b = m_config->effectiveHotkeyBindings();
    qInfo() << "[Config] binding count:" << b.size() << "paused:" << m_config->getPaused();
    m_keyboardHooker->updateBindings(b);
    m_keyboardHooker->setPaused(m_config->getPaused());
    m_taskbarHooker->setPaused(m_config->getPaused());
    m_keyboardHooker->resetActivationModifiers();
}
