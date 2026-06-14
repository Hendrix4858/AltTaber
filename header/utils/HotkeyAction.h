#ifndef WIN_SWITCHER_HOTKEYACTION_H
#define WIN_SWITCHER_HOTKEYACTION_H

#include <QString>
#include <QList>
#include <QMap>
#include <QJsonObject>
#include <QCoreApplication>
#include <Qt>
#include <QDebug>
#include <windows.h>

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

struct HotkeyBinding {
    enum class KeyMode { Logical, Physical };

    Qt::KeyboardModifiers modifiers = Qt::NoModifier;
    quint32 vkCode = 0;
    quint32 scanCode = 0;
    bool extended = false;
    KeyMode mode = KeyMode::Logical;

    bool isValid() const {
        if (mode == KeyMode::Physical)
            return scanCode != 0 || vkCode != 0;
        return vkCode != 0;
    }

    bool matches(quint32 testVk, Qt::KeyboardModifiers testMods) const {
        if (modifiers != testMods) return false;
        if (mode == KeyMode::Physical && scanCode != 0)
            return vkCode == testVk;
        return vkCode == testVk;
    }

    bool matchesPhysical(quint32 testVk, quint32 testScan, bool testExtended, Qt::KeyboardModifiers testMods) const {
        if (mode != KeyMode::Physical) return matches(testVk, testMods);
        if (modifiers != testMods) return false;
        if (scanCode) return scanCode == testScan && extended == testExtended;
        return vkCode == testVk;
    }

    bool isSingleLetter() const {
        return vkCode >= 'A' && vkCode <= 'Z' && modifiers == Qt::NoModifier && mode == KeyMode::Logical;
    }

    QString toString() const;
    static HotkeyBinding fromString(const QString& str);
    QJsonObject toJson() const;
    static HotkeyBinding fromJson(const QJsonObject& obj);

    bool operator==(const HotkeyBinding& o) const {
        if (mode == KeyMode::Physical && o.mode == KeyMode::Physical)
            return scanCode == o.scanCode && extended == o.extended && modifiers == o.modifiers;
        return modifiers == o.modifiers && vkCode == o.vkCode;
    }
    bool operator!=(const HotkeyBinding& o) const { return !(*this == o); }
};

using HotkeyBindings = QMap<HotkeyAction, QList<HotkeyBinding>>;

inline HotkeyBinding makePhysicalBinding(Qt::KeyboardModifiers mods, quint32 vk, quint32 sc, bool ext = false) {
    HotkeyBinding b;
    b.modifiers = mods;
    b.vkCode = vk;
    b.scanCode = sc;
    b.extended = ext;
    b.mode = HotkeyBinding::KeyMode::Physical;
    return b;
}

inline HotkeyBindings defaultHotkeyBindings() {
    HotkeyBindings defaults;
    defaults[HotkeyAction::SwitchToNextWindow]     = {makePhysicalBinding(Qt::AltModifier, VK_TAB, 0x0F)};
    defaults[HotkeyAction::CycleProcessWindows]     = {};
    defaults[HotkeyAction::SwitchProcessWindow]     = {};
    defaults[HotkeyAction::EnterGroupMode]          = {};
    defaults[HotkeyAction::CycleForward]            = {makePhysicalBinding(Qt::NoModifier, VK_TAB, 0x0F)};
    defaults[HotkeyAction::CycleBackward]           = {makePhysicalBinding(Qt::ShiftModifier, VK_TAB, 0x0F)};
    defaults[HotkeyAction::MoveSelectionUp]         = {};
    defaults[HotkeyAction::MoveSelectionDown]       = {};
    defaults[HotkeyAction::ActivateSelected]        = {};
    defaults[HotkeyAction::DismissSwitcher]         = {makePhysicalBinding(Qt::NoModifier, VK_ESCAPE, 0x01)};
    defaults[HotkeyAction::TogglePause]             = {};
    defaults[HotkeyAction::SwitchToPreviousWindow]  = {
        makePhysicalBinding(Qt::AltModifier | Qt::ShiftModifier, VK_TAB, 0x0F)
    };
    defaults[HotkeyAction::ShowSwitcherStayOpen]    = {};
    return defaults;
}

void normalizeHotkeyBindings(HotkeyBindings& bindings);

namespace HotkeyStrings {

inline QString vkCodeToString(quint32 vk) {
    if (vk >= 'A' && vk <= 'Z')
        return QChar(vk);
    if (vk >= '0' && vk <= '9')
        return QChar(vk);
    if (vk >= VK_F1 && vk <= VK_F24)
        return QStringLiteral("F%1").arg(vk - VK_F1 + 1);

    switch (vk) {
        case VK_TAB:      return QStringLiteral("Tab");
        case VK_RETURN:   return QStringLiteral("Enter");
        case VK_ESCAPE:   return QStringLiteral("Escape");
        case VK_LEFT:     return QStringLiteral("Left");
        case VK_RIGHT:    return QStringLiteral("Right");
        case VK_UP:       return QStringLiteral("Up");
        case VK_DOWN:     return QStringLiteral("Down");
        case VK_OEM_3:    return QStringLiteral("Grave");
        case VK_SPACE:    return QStringLiteral("Space");
        case VK_BACK:     return QStringLiteral("Backspace");
        case VK_DELETE:   return QStringLiteral("Delete");
        case VK_HOME:     return QStringLiteral("Home");
        case VK_END:      return QStringLiteral("End");
        case VK_PRIOR:    return QStringLiteral("PageUp");
        case VK_NEXT:     return QStringLiteral("PageDown");
        case VK_OEM_PLUS: return QStringLiteral("+");
        case VK_OEM_MINUS: return QStringLiteral("-");
        case VK_OEM_COMMA: return QStringLiteral(",");
        case VK_OEM_PERIOD: return QStringLiteral(".");
        case VK_OEM_1:    return QStringLiteral(";");
        case VK_OEM_2:    return QStringLiteral("/");
        case VK_OEM_4:    return QStringLiteral("[");
        case VK_OEM_6:    return QStringLiteral("]");
        case VK_OEM_7:    return QStringLiteral("'");
        case VK_OEM_5:    return QStringLiteral("\\");
    }
    return {};
}

inline quint32 stringToVkCode(const QString& str) {
    if (str.length() == 1) {
        QChar c = str[0];
        if (c >= 'A' && c <= 'Z') return c.unicode();
        if (c >= '0' && c <= '9') return c.unicode();
        if (c == '+') return VK_OEM_PLUS;
        if (c == '-') return VK_OEM_MINUS;
        if (c == ',') return VK_OEM_COMMA;
        if (c == '.') return VK_OEM_PERIOD;
        if (c == ';') return VK_OEM_1;
        if (c == '/') return VK_OEM_2;
        if (c == '[') return VK_OEM_4;
        if (c == ']') return VK_OEM_6;
        if (c == '\'') return VK_OEM_7;
        if (c == '\\') return VK_OEM_5;
    }

    static const QMap<QString, quint32> map = []() {
        QMap<QString, quint32> m;
        m.insert("Tab", VK_TAB); m.insert("Enter", VK_RETURN);
        m.insert("Escape", VK_ESCAPE);
        m.insert("Left", VK_LEFT); m.insert("Right", VK_RIGHT);
        m.insert("Up", VK_UP); m.insert("Down", VK_DOWN);
        m.insert("Grave", VK_OEM_3); m.insert("Space", VK_SPACE);
        m.insert("Backspace", VK_BACK); m.insert("Delete", VK_DELETE);
        m.insert("Home", VK_HOME); m.insert("End", VK_END);
        m.insert("PageUp", VK_PRIOR); m.insert("PageDown", VK_NEXT);
        return m;
    }();
    if (str.startsWith('F') && str.length() <= 3) {
        bool ok;
        int n = str.mid(1).toInt(&ok);
        if (ok && n >= 1 && n <= 24)
            return VK_F1 + n - 1;
    }
    return map.value(str, 0);
}

inline QString modifierToString(Qt::KeyboardModifiers mods) {
    QStringList parts;
    if (mods & Qt::ControlModifier) parts << QStringLiteral("Ctrl");
    if (mods & Qt::AltModifier)     parts << QStringLiteral("Alt");
    if (mods & Qt::ShiftModifier)   parts << QStringLiteral("Shift");
    if (mods & Qt::MetaModifier)    parts << QStringLiteral("Win");
    return parts.join('+');
}

inline Qt::KeyboardModifiers stringToModifiers(const QString& str) {
    Qt::KeyboardModifiers mods = Qt::NoModifier;
    const auto parts = str.split('+');
    for (const auto& part : parts) {
        if (part == "Ctrl")  mods |= Qt::ControlModifier;
        if (part == "Alt")   mods |= Qt::AltModifier;
        if (part == "Shift") mods |= Qt::ShiftModifier;
        if (part == "Win")   mods |= Qt::MetaModifier;
    }
    return mods;
}

inline QString getKeyNameText(quint32 sc, bool ext) {
    LONG lParam = (sc << 16) | (ext ? (1 << 24) : 0);
    WCHAR name[64] = {};
    int len = GetKeyNameTextW(lParam, name, 64);
    return len > 0 ? QString::fromWCharArray(name, len) : QString();
}

} // namespace HotkeyStrings

inline QString HotkeyBinding::toString() const {
    QString keyStr;
    if (mode == KeyMode::Physical && scanCode != 0)
        keyStr = HotkeyStrings::getKeyNameText(scanCode, extended);
    if (keyStr.isEmpty())
        keyStr = HotkeyStrings::vkCodeToString(vkCode);
    if (keyStr.isEmpty()) return {};
    QString modStr = HotkeyStrings::modifierToString(modifiers);
    if (modStr.isEmpty()) return keyStr;
    return modStr + '+' + keyStr;
}

inline HotkeyBinding HotkeyBinding::fromString(const QString& str) {
    HotkeyBinding b;
    int plusIdx = -1;
    for (int i = str.length() - 1; i >= 0; --i) {
        if (str[i] == '+') {
            plusIdx = i;
            break;
        }
    }
    if (plusIdx > 0) {
        // Has modifiers + key
        b.modifiers = HotkeyStrings::stringToModifiers(str.left(plusIdx));
        b.vkCode = HotkeyStrings::stringToVkCode(str.mid(plusIdx + 1));
    } else {
        // No modifiers, just a key
        b.vkCode = HotkeyStrings::stringToVkCode(str);
    }
    return b;
}

inline QJsonObject HotkeyBinding::toJson() const {
    QJsonObject obj;
    QString modStr = HotkeyStrings::modifierToString(modifiers);
    if (!modStr.isEmpty()) obj["modifiers"] = modStr;
    obj["vkCode"] = (int)vkCode;
    if (mode == KeyMode::Physical) {
        obj["mode"] = QStringLiteral("physical");
        if (scanCode) obj["scanCode"] = (int)scanCode;
        if (extended) obj["extended"] = true;
    } else {
        obj["mode"] = QStringLiteral("logical");
    }
    return obj;
}

inline HotkeyBinding HotkeyBinding::fromJson(const QJsonObject& obj) {
    HotkeyBinding b;
    b.modifiers = HotkeyStrings::stringToModifiers(obj["modifiers"].toString());
    b.vkCode = obj["vkCode"].toInt();
    b.scanCode = obj["scanCode"].toInt();
    b.extended = obj["extended"].toBool();
    if (obj["mode"].toString() == QLatin1String("physical"))
        b.mode = KeyMode::Physical;
    return b;
}

// Helper: convert Qt key to Windows VK code
inline quint32 qtKeyToVkCode(int qtKey) {
    if (qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z)
        return qtKey - Qt::Key_A + 'A';
    if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9)
        return qtKey - Qt::Key_0 + '0';
    if (qtKey >= Qt::Key_F1 && qtKey <= Qt::Key_F24)
        return VK_F1 + (qtKey - Qt::Key_F1);

    switch (qtKey) {
        case Qt::Key_Tab:       return VK_TAB;
        case Qt::Key_Return:
        case Qt::Key_Enter:     return VK_RETURN;
        case Qt::Key_Escape:    return VK_ESCAPE;
        case Qt::Key_Left:      return VK_LEFT;
        case Qt::Key_Right:     return VK_RIGHT;
        case Qt::Key_Up:        return VK_UP;
        case Qt::Key_Down:      return VK_DOWN;
        case Qt::Key_QuoteLeft:
        case Qt::Key_AsciiTilde: return VK_OEM_3;
        case Qt::Key_Space:     return VK_SPACE;
        case Qt::Key_Backspace: return VK_BACK;
        case Qt::Key_Delete:    return VK_DELETE;
        case Qt::Key_Home:      return VK_HOME;
        case Qt::Key_End:       return VK_END;
        case Qt::Key_PageUp:    return VK_PRIOR;
        case Qt::Key_PageDown:  return VK_NEXT;
        case Qt::Key_Equal:
        case Qt::Key_Plus:      return VK_OEM_PLUS;
        case Qt::Key_Minus:
        case Qt::Key_Underscore: return VK_OEM_MINUS;
        case Qt::Key_Comma:
        case Qt::Key_Less:      return VK_OEM_COMMA;
        case Qt::Key_Period:
        case Qt::Key_Greater:   return VK_OEM_PERIOD;
        case Qt::Key_Semicolon:
        case Qt::Key_Colon:     return VK_OEM_1;
        case Qt::Key_Slash:
        case Qt::Key_Question:  return VK_OEM_2;
        case Qt::Key_BracketLeft:
        case Qt::Key_BraceLeft: return VK_OEM_4;
        case Qt::Key_BracketRight:
        case Qt::Key_BraceRight: return VK_OEM_6;
        case Qt::Key_Apostrophe:
        case Qt::Key_QuoteDbl:  return VK_OEM_7;
        case Qt::Key_Backslash:
        case Qt::Key_Bar:       return VK_OEM_5;
    }
    return 0;
}

// Convert Windows VK code to Qt key code
inline int vkCodeToQtKey(quint32 vk) {
    if (vk >= 'A' && vk <= 'Z')
        return Qt::Key_A + (vk - 'A');
    if (vk >= '0' && vk <= '9')
        return Qt::Key_0 + (vk - '0');
    if (vk >= VK_F1 && vk <= VK_F24)
        return Qt::Key_F1 + (vk - VK_F1);

    switch (vk) {
        case VK_TAB:      return Qt::Key_Tab;
        case VK_RETURN:   return Qt::Key_Return;
        case VK_ESCAPE:   return Qt::Key_Escape;
        case VK_LEFT:     return Qt::Key_Left;
        case VK_RIGHT:    return Qt::Key_Right;
        case VK_UP:       return Qt::Key_Up;
        case VK_DOWN:     return Qt::Key_Down;
        case VK_OEM_3:    return Qt::Key_QuoteLeft;
        case VK_SPACE:    return Qt::Key_Space;
        case VK_BACK:     return Qt::Key_Backspace;
        case VK_DELETE:   return Qt::Key_Delete;
        case VK_HOME:     return Qt::Key_Home;
        case VK_END:      return Qt::Key_End;
        case VK_PRIOR:    return Qt::Key_PageUp;
        case VK_NEXT:     return Qt::Key_PageDown;
        case VK_OEM_PLUS: return Qt::Key_Equal;
        case VK_OEM_MINUS:return Qt::Key_Minus;
        case VK_OEM_COMMA:return Qt::Key_Comma;
        case VK_OEM_PERIOD:return Qt::Key_Period;
        case VK_OEM_1:    return Qt::Key_Semicolon;
        case VK_OEM_2:    return Qt::Key_Slash;
        case VK_OEM_4:    return Qt::Key_BracketLeft;
        case VK_OEM_6:    return Qt::Key_BracketRight;
        case VK_OEM_7:    return Qt::Key_Apostrophe;
        case VK_OEM_5:    return Qt::Key_Backslash;
    }
    return 0;
}

#endif //WIN_SWITCHER_HOTKEYACTION_H
