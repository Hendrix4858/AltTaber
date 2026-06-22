#include "ActionRouter.h"
#include "OverlayController.h"
#include "SelectionController.h"
#include "GroupWindowCycler.h"
#include "WindowManager.h"
#include "utils/Util.h"
#include "core/ConfigManager.h"
#include <QDebug>

ActionRouter::ActionRouter(QObject* parent)
    : QObject(parent) {}

void ActionRouter::setOverlayController(OverlayController* ctrl) {
    m_overlayCtrl = ctrl;
}

void ActionRouter::setSelectionController(SelectionController* selCtrl) {
    m_selectCtrl = selCtrl;
}

void ActionRouter::setGroupWindowCycler(GroupWindowCycler* cycler) {
    m_groupCycler = cycler;
}

void ActionRouter::setWindowManager(WindowManager* wm) {
    m_windowManager = wm;
}

void ActionRouter::routeGlobalAction(HotkeyAction action, Qt::KeyboardModifiers modifiers) {
    qInfo() << "[ActionRouter] routeGlobalAction" << hotkeyActionName(action);

    switch (action) {
    case HotkeyAction::SwitchToNextWindow:
    case HotkeyAction::SwitchToPreviousWindow:
    case HotkeyAction::ShowSwitcherStayOpen:
        if (m_overlayCtrl)
            m_overlayCtrl->handleGlobalAction(action, modifiers);
        break;

    case HotkeyAction::CycleProcessWindows:
        if (m_overlayCtrl && m_selectCtrl && m_windowManager) {
            auto foregroundHwnd = GetForegroundWindow();
            auto targetExePath = Util::getWindowProcessPath(foregroundHwnd);
            auto hwnds = m_windowManager->filteredHwndsForExe(targetExePath);
            if (hwnds.size() >= 2) {
                m_selectCtrl->resetAll();
                auto meta = getActionMetadata(action);
                if (meta.lifecycle == HotkeyLifecycle::OverlaySession)
                    m_overlayCtrl->setSessionInfo({action, meta.endTrigger});
                m_overlayCtrl->handleIntent(OverlayIntent::ShowSwitcher);
                m_selectCtrl->tryEnterGroupForWindow(foregroundHwnd);
            }
        }
        break;

    case HotkeyAction::SwitchProcessWindow:
        if (m_groupCycler && m_windowManager) {
            auto foregroundHwnd = GetForegroundWindow();
            auto& order = m_groupCycler->groupWindowOrder();
            if (order.isEmpty()) {
                auto targetExePath = Util::getWindowProcessPath(foregroundHwnd);
                order = m_windowManager->filteredHwndsForExe(targetExePath);
            }
            bool forward = !(modifiers & Qt::ShiftModifier);
            if (auto nextWin = GroupWindowCycler::rotateWindow(order, foregroundHwnd, forward))
                Util::switchToWindow(nextWin, true);
        }
        break;

    case HotkeyAction::ExpandGroup:
        if (m_selectCtrl)
            m_selectCtrl->handleOverlayAction(HotkeyAction::ExpandGroup, modifiers);
        break;

    case HotkeyAction::TogglePause:
        cfg().setPaused(!cfg().getPaused());
        break;

    default:
        break;
    }
}
