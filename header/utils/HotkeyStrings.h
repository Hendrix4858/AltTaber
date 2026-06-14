#ifndef WIN_SWITCHER_HOTKEYSTRINGS_H
#define WIN_SWITCHER_HOTKEYSTRINGS_H

#include <QString>
#include <Qt>
#include <windows.h>

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

#endif //WIN_SWITCHER_HOTKEYSTRINGS_H
