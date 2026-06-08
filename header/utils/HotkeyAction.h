#ifndef WIN_SWITCHER_HOTKEYACTION_H
#define WIN_SWITCHER_HOTKEYACTION_H

#include <QString>
#include <QList>
#include <QMap>
#include <Qt>
#include <QDebug>
#include <windows.h>

enum class HotkeyAction {
    ShowSwitcher,
    EnterGroupMode,
    CycleForward,
    CycleBackward,
    MoveSelectionUp,
    MoveSelectionDown,
    ActivateSelected,
    DismissSwitcher,
    TogglePause,
};

constexpr inline HotkeyAction AllActions[] = {
    HotkeyAction::ShowSwitcher,
    HotkeyAction::EnterGroupMode,
    HotkeyAction::CycleForward,
    HotkeyAction::CycleBackward,
    HotkeyAction::MoveSelectionUp,
    HotkeyAction::MoveSelectionDown,
    HotkeyAction::ActivateSelected,
    HotkeyAction::DismissSwitcher,
    HotkeyAction::TogglePause,
};

inline QString hotkeyActionName(HotkeyAction action) {
    switch (action) {
        case HotkeyAction::ShowSwitcher:     return "ShowSwitcher";
        case HotkeyAction::EnterGroupMode:   return "EnterGroupMode";
        case HotkeyAction::CycleForward:     return "CycleForward";
        case HotkeyAction::CycleBackward:    return "CycleBackward";
        case HotkeyAction::MoveSelectionUp:  return "MoveSelectionUp";
        case HotkeyAction::MoveSelectionDown: return "MoveSelectionDown";
        case HotkeyAction::ActivateSelected: return "ActivateSelected";
        case HotkeyAction::DismissSwitcher:  return "DismissSwitcher";
        case HotkeyAction::TogglePause:      return "TogglePause";
    }
    return {};
}

inline HotkeyAction hotkeyActionFromName(const QString& name) {
    static const QMap<QString, HotkeyAction> map = []() {
        QMap<QString, HotkeyAction> m;
        m.insert("ShowSwitcher",     HotkeyAction::ShowSwitcher);
        m.insert("EnterGroupMode",   HotkeyAction::EnterGroupMode);
        m.insert("CycleForward",     HotkeyAction::CycleForward);
        m.insert("CycleBackward",    HotkeyAction::CycleBackward);
        m.insert("MoveSelectionUp",  HotkeyAction::MoveSelectionUp);
        m.insert("MoveSelectionDown", HotkeyAction::MoveSelectionDown);
        m.insert("ActivateSelected", HotkeyAction::ActivateSelected);
        m.insert("DismissSwitcher",  HotkeyAction::DismissSwitcher);
        m.insert("TogglePause",      HotkeyAction::TogglePause);
        return m;
    }();
    return map.value(name);
}

inline QString hotkeyActionDisplayName(HotkeyAction action) {
    switch (action) {
        case HotkeyAction::ShowSwitcher:     return QStringLiteral("\u663E\u793A\u8986\u76D6\u5C42");
        case HotkeyAction::EnterGroupMode:   return QStringLiteral("\u8FDB\u5165\u7EC4\u6A21\u5F0F/\u5207\u540C\u8FDB\u7A0B\u7A97\u53E3");
        case HotkeyAction::CycleForward:     return QStringLiteral("\u524D\u8FDB\u9009\u62E9");
        case HotkeyAction::CycleBackward:    return QStringLiteral("\u540E\u9000\u9009\u62E9");
        case HotkeyAction::MoveSelectionUp:  return QStringLiteral("\u5411\u4E0A\u79FB\u52A8\u9009\u62E9");
        case HotkeyAction::MoveSelectionDown: return QStringLiteral("\u5411\u4E0B\u79FB\u52A8\u9009\u62E9");
        case HotkeyAction::ActivateSelected: return QStringLiteral("\u6FC0\u6D3B\u9009\u4E2D\u7A97\u53E3");
        case HotkeyAction::DismissSwitcher:  return QStringLiteral("\u5173\u95ED\u8986\u76D6\u5C42");
        case HotkeyAction::TogglePause:      return QStringLiteral("\u5207\u6362\u6682\u505C");
    }
    return {};
}

struct HotkeyBinding {
    Qt::KeyboardModifiers modifiers = Qt::NoModifier;
    quint32 vkCode = 0;

    bool isValid() const { return vkCode != 0; }

    bool matches(quint32 testVk, Qt::KeyboardModifiers testMods) const {
        return vkCode == testVk && modifiers == testMods;
    }

    bool isSingleLetter() const {
        return vkCode >= 'A' && vkCode <= 'Z' && modifiers == Qt::NoModifier;
    }

    QString toString() const;
    static HotkeyBinding fromString(const QString& str);

    bool operator==(const HotkeyBinding& o) const {
        return modifiers == o.modifiers && vkCode == o.vkCode;
    }
    bool operator!=(const HotkeyBinding& o) const { return !(*this == o); }
};

using HotkeyBindings = QMap<HotkeyAction, QList<HotkeyBinding>>;

inline HotkeyBindings defaultHotkeyBindings() {
    HotkeyBindings defaults;
    defaults[HotkeyAction::ShowSwitcher]     = {HotkeyBinding::fromString("Alt+Tab")};
    defaults[HotkeyAction::EnterGroupMode]   = {HotkeyBinding::fromString("Alt+Grave")};
    defaults[HotkeyAction::CycleForward]     = {HotkeyBinding::fromString("Tab")};
    defaults[HotkeyAction::CycleBackward]    = {HotkeyBinding::fromString("Shift+Tab")};
    defaults[HotkeyAction::MoveSelectionUp]  = {HotkeyBinding::fromString("Up")};
    defaults[HotkeyAction::MoveSelectionDown] = {HotkeyBinding::fromString("Down")};
    defaults[HotkeyAction::ActivateSelected] = {HotkeyBinding::fromString("Enter")};
    defaults[HotkeyAction::DismissSwitcher]  = {HotkeyBinding::fromString("Escape")};
    defaults[HotkeyAction::TogglePause]      = {};
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

} // namespace HotkeyStrings

inline QString HotkeyBinding::toString() const {
    QString keyStr = HotkeyStrings::vkCodeToString(vkCode);
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
        case Qt::Key_QuoteLeft: return VK_OEM_3;
        case Qt::Key_Space:     return VK_SPACE;
        case Qt::Key_Backspace: return VK_BACK;
        case Qt::Key_Delete:    return VK_DELETE;
        case Qt::Key_Home:      return VK_HOME;
        case Qt::Key_End:       return VK_END;
        case Qt::Key_PageUp:    return VK_PRIOR;
        case Qt::Key_PageDown:  return VK_NEXT;
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
    }
    return 0;
}

#endif //WIN_SWITCHER_HOTKEYACTION_H
