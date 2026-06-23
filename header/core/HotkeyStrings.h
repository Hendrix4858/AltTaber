#ifndef WIN_SWITCHER_HOTKEYSTRINGS_H
#define WIN_SWITCHER_HOTKEYSTRINGS_H

#include <QString>
#include <Qt>
#include <windows.h>

namespace HotkeyStrings {

struct KeyMapping { quint32 vk; int qtKey; const char* name; };
static constexpr KeyMapping keyMappings[] = {
    { VK_TAB,      Qt::Key_Tab,       "Tab" },
    { VK_RETURN,   Qt::Key_Return,    "Enter" },
    { VK_ESCAPE,   Qt::Key_Escape,    "Escape" },
    { VK_LEFT,     Qt::Key_Left,      "Left" },
    { VK_RIGHT,    Qt::Key_Right,     "Right" },
    { VK_UP,       Qt::Key_Up,        "Up" },
    { VK_DOWN,     Qt::Key_Down,      "Down" },
    { VK_OEM_3,    Qt::Key_QuoteLeft, "Grave" },
    { VK_SPACE,    Qt::Key_Space,     "Space" },
    { VK_BACK,     Qt::Key_Backspace, "Backspace" },
    { VK_DELETE,   Qt::Key_Delete,    "Delete" },
    { VK_HOME,     Qt::Key_Home,      "Home" },
    { VK_END,      Qt::Key_End,       "End" },
    { VK_PRIOR,    Qt::Key_PageUp,    "PageUp" },
    { VK_NEXT,     Qt::Key_PageDown,  "PageDown" },
    { VK_OEM_PLUS, Qt::Key_Equal,     "+" },
    { VK_OEM_MINUS,Qt::Key_Minus,     "-" },
    { VK_OEM_COMMA,Qt::Key_Comma,     "," },
    { VK_OEM_PERIOD,Qt::Key_Period,   "." },
    { VK_OEM_1,    Qt::Key_Semicolon, ";" },
    { VK_OEM_2,    Qt::Key_Slash,     "/" },
    { VK_OEM_4,    Qt::Key_BracketLeft, "[" },
    { VK_OEM_6,    Qt::Key_BracketRight, "]" },
    { VK_OEM_7,    Qt::Key_Apostrophe, "'" },
    { VK_OEM_5,    Qt::Key_Backslash, "\\" },
};

inline QString vkCodeToString(quint32 vk) {
    if (vk >= 'A' && vk <= 'Z')
        return QChar(vk);
    if (vk >= '0' && vk <= '9')
        return QChar(vk);
    if (vk >= VK_F1 && vk <= VK_F24)
        return QStringLiteral("F%1").arg(vk - VK_F1 + 1);
    for (const auto& m : keyMappings)
        if (m.vk == vk) return QLatin1String(m.name);
    return {};
}

inline quint32 stringToVkCode(const QString& str) {
    if (str.length() == 1) {
        QChar c = str[0];
        if (c >= 'A' && c <= 'Z') return c.unicode();
        if (c >= '0' && c <= '9') return c.unicode();
    }
    if (str.startsWith('F') && str.length() <= 3) {
        bool ok;
        int n = str.mid(1).toInt(&ok);
        if (ok && n >= 1 && n <= 24)
            return VK_F1 + n - 1;
    }
    for (const auto& m : keyMappings)
        if (str == QLatin1String(m.name)) return m.vk;
    return 0;
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

inline quint32 qtKeyToVkCode(int qtKey) {
    if (qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z)
        return qtKey - Qt::Key_A + 'A';
    if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9)
        return qtKey - Qt::Key_0 + '0';
    if (qtKey >= Qt::Key_F1 && qtKey <= Qt::Key_F24)
        return VK_F1 + (qtKey - Qt::Key_F1);

    if (qtKey == Qt::Key_Enter || qtKey == Qt::Key_Return)
        return VK_RETURN;
    if (qtKey == Qt::Key_QuoteLeft || qtKey == Qt::Key_AsciiTilde)
        return VK_OEM_3;
    if (qtKey == Qt::Key_Equal || qtKey == Qt::Key_Plus)
        return VK_OEM_PLUS;
    if (qtKey == Qt::Key_Minus || qtKey == Qt::Key_Underscore)
        return VK_OEM_MINUS;
    if (qtKey == Qt::Key_Comma || qtKey == Qt::Key_Less)
        return VK_OEM_COMMA;
    if (qtKey == Qt::Key_Period || qtKey == Qt::Key_Greater)
        return VK_OEM_PERIOD;
    if (qtKey == Qt::Key_Semicolon || qtKey == Qt::Key_Colon)
        return VK_OEM_1;
    if (qtKey == Qt::Key_Slash || qtKey == Qt::Key_Question)
        return VK_OEM_2;
    if (qtKey == Qt::Key_BracketLeft || qtKey == Qt::Key_BraceLeft)
        return VK_OEM_4;
    if (qtKey == Qt::Key_BracketRight || qtKey == Qt::Key_BraceRight)
        return VK_OEM_6;
    if (qtKey == Qt::Key_Apostrophe || qtKey == Qt::Key_QuoteDbl)
        return VK_OEM_7;
    if (qtKey == Qt::Key_Backslash || qtKey == Qt::Key_Bar)
        return VK_OEM_5;

    for (const auto& m : HotkeyStrings::keyMappings)
        if (m.qtKey == qtKey) return m.vk;
    return 0;
}

inline int vkCodeToQtKey(quint32 vk) {
    if (vk >= 'A' && vk <= 'Z')
        return Qt::Key_A + (vk - 'A');
    if (vk >= '0' && vk <= '9')
        return Qt::Key_0 + (vk - '0');
    if (vk >= VK_F1 && vk <= VK_F24)
        return Qt::Key_F1 + (vk - VK_F1);
    for (const auto& m : HotkeyStrings::keyMappings)
        if (m.vk == vk) return m.qtKey;
    return 0;
}

#endif //WIN_SWITCHER_HOTKEYSTRINGS_H
