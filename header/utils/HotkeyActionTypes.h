#ifndef WIN_SWITCHER_HOTKEYACTIONTYPES_H
#define WIN_SWITCHER_HOTKEYACTIONTYPES_H

#include <QString>
#include <QCoreApplication>

enum class HotkeyAction {
    SwitchToNextWindow,
    CycleProcessWindows,
    SwitchProcessWindow,
    EnterGroupMode,
    CycleForward,
    CycleBackward,
    MoveSelectionUp,
    MoveSelectionDown,
    ActivateSelected,
    DismissSwitcher,
    TogglePause,
    SwitchToPreviousWindow,
    ShowSwitcherStayOpen,
};

constexpr inline HotkeyAction AllActions[] = {
    HotkeyAction::SwitchToNextWindow,
    HotkeyAction::CycleProcessWindows,
    HotkeyAction::SwitchProcessWindow,
    HotkeyAction::EnterGroupMode,
    HotkeyAction::CycleForward,
    HotkeyAction::CycleBackward,
    HotkeyAction::MoveSelectionUp,
    HotkeyAction::MoveSelectionDown,
    HotkeyAction::ActivateSelected,
    HotkeyAction::DismissSwitcher,
    HotkeyAction::TogglePause,
    HotkeyAction::SwitchToPreviousWindow,
    HotkeyAction::ShowSwitcherStayOpen,
};

enum class HotkeyScope { Global, Overlay };

inline HotkeyScope hotkeyActionScope(HotkeyAction action) {
    switch (action) {
        case HotkeyAction::SwitchToNextWindow:
        case HotkeyAction::CycleProcessWindows:
        case HotkeyAction::SwitchProcessWindow:
        case HotkeyAction::TogglePause:
        case HotkeyAction::SwitchToPreviousWindow:
        case HotkeyAction::ShowSwitcherStayOpen:
            return HotkeyScope::Global;
        default:
            return HotkeyScope::Overlay;
    }
}

enum class HotkeyLifecycle {
    Instant,
    OverlaySession
};

enum class SessionEndTrigger {
    ModifierRelease,
    ExplicitAction,
    FocusLost
};

struct ActionMetadata {
    HotkeyLifecycle lifecycle = HotkeyLifecycle::Instant;
    SessionEndTrigger endTrigger = SessionEndTrigger::ExplicitAction;
};

inline ActionMetadata getActionMetadata(HotkeyAction action) {
    switch (action) {
        case HotkeyAction::SwitchToNextWindow:
        case HotkeyAction::SwitchToPreviousWindow:
        case HotkeyAction::CycleProcessWindows:
            return {HotkeyLifecycle::OverlaySession, SessionEndTrigger::ModifierRelease};
        case HotkeyAction::TogglePause:
        case HotkeyAction::SwitchProcessWindow:
            return {HotkeyLifecycle::Instant, SessionEndTrigger::ExplicitAction};
        case HotkeyAction::ShowSwitcherStayOpen:
            return {HotkeyLifecycle::OverlaySession, SessionEndTrigger::ExplicitAction};
        default:
            return {HotkeyLifecycle::OverlaySession, SessionEndTrigger::ExplicitAction};
    }
}

inline QString hotkeyActionName(HotkeyAction action) {
    switch (action) {
        case HotkeyAction::SwitchToNextWindow:     return "SwitchToNextWindow";
        case HotkeyAction::CycleProcessWindows:     return "CycleProcessWindows";
        case HotkeyAction::SwitchProcessWindow:     return "SwitchProcessWindow";
        case HotkeyAction::EnterGroupMode:          return "EnterGroupMode";
        case HotkeyAction::CycleForward:            return "CycleForward";
        case HotkeyAction::CycleBackward:           return "CycleBackward";
        case HotkeyAction::MoveSelectionUp:         return "MoveSelectionUp";
        case HotkeyAction::MoveSelectionDown:       return "MoveSelectionDown";
        case HotkeyAction::ActivateSelected:        return "ActivateSelected";
        case HotkeyAction::DismissSwitcher:         return "DismissSwitcher";
        case HotkeyAction::TogglePause:             return "TogglePause";
        case HotkeyAction::SwitchToPreviousWindow:  return "SwitchToPreviousWindow";
        case HotkeyAction::ShowSwitcherStayOpen:    return "ShowSwitcherStayOpen";
    }
    return {};
}

inline HotkeyAction hotkeyActionFromName(const QString& name) {
    static const QMap<QString, HotkeyAction> map = []() {
        QMap<QString, HotkeyAction> m;
        m.insert("SwitchToNextWindow",     HotkeyAction::SwitchToNextWindow);
        m.insert("CycleProcessWindows",    HotkeyAction::CycleProcessWindows);
        m.insert("SwitchProcessWindow",    HotkeyAction::SwitchProcessWindow);
        m.insert("EnterGroupMode",         HotkeyAction::EnterGroupMode);
        m.insert("CycleForward",           HotkeyAction::CycleForward);
        m.insert("CycleBackward",          HotkeyAction::CycleBackward);
        m.insert("MoveSelectionUp",        HotkeyAction::MoveSelectionUp);
        m.insert("MoveSelectionDown",      HotkeyAction::MoveSelectionDown);
        m.insert("ActivateSelected",       HotkeyAction::ActivateSelected);
        m.insert("DismissSwitcher",        HotkeyAction::DismissSwitcher);
        m.insert("TogglePause",            HotkeyAction::TogglePause);
        m.insert("SwitchToPreviousWindow", HotkeyAction::SwitchToPreviousWindow);
        m.insert("ShowSwitcherStayOpen",   HotkeyAction::ShowSwitcherStayOpen);
        return m;
    }();
    return map.value(name);
}

inline QString hotkeyActionDisplayName(HotkeyAction action) {
    switch (action) {
        case HotkeyAction::SwitchToNextWindow:     return QCoreApplication::translate("HotkeyAction", "Switch to Next Window");
        case HotkeyAction::CycleProcessWindows:    return QCoreApplication::translate("HotkeyAction", "Show Process Window List");
        case HotkeyAction::SwitchProcessWindow:    return QCoreApplication::translate("HotkeyAction", "Switch Process Window");
        case HotkeyAction::EnterGroupMode:         return QCoreApplication::translate("HotkeyAction", "Enter Process Window List");
        case HotkeyAction::CycleForward:           return QCoreApplication::translate("HotkeyAction", "Cycle Forward");
        case HotkeyAction::CycleBackward:          return QCoreApplication::translate("HotkeyAction", "Cycle Backward");
        case HotkeyAction::MoveSelectionUp:        return QCoreApplication::translate("HotkeyAction", "Move Selection Up");
        case HotkeyAction::MoveSelectionDown:      return QCoreApplication::translate("HotkeyAction", "Move Selection Down");
        case HotkeyAction::ActivateSelected:       return QCoreApplication::translate("HotkeyAction", "Activate Selected");
        case HotkeyAction::DismissSwitcher:        return QCoreApplication::translate("HotkeyAction", "Dismiss Switcher");
        case HotkeyAction::TogglePause:            return QCoreApplication::translate("HotkeyAction", "Toggle Pause");
        case HotkeyAction::SwitchToPreviousWindow: return QCoreApplication::translate("HotkeyAction", "Switch to Previous Window");
        case HotkeyAction::ShowSwitcherStayOpen:   return QCoreApplication::translate("HotkeyAction", "Keep Window Displayed");
    }
    return {};
}

#endif //WIN_SWITCHER_HOTKEYACTIONTYPES_H
