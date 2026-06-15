#ifndef WIN_SWITCHER_HOTKEYBINDING_H
#define WIN_SWITCHER_HOTKEYBINDING_H

#include <QList>
#include <QMap>
#include <QJsonObject>
#include <QDebug>
#include "core/HotkeyActionTypes.h"
#include "core/HotkeyStrings.h"

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
    defaults[HotkeyAction::CycleProcessWindows]     = {makePhysicalBinding(Qt::AltModifier, VK_OEM_3, 0x29)};
    defaults[HotkeyAction::SwitchProcessWindow]     = {};
    defaults[HotkeyAction::EnterGroupMode]          = {makePhysicalBinding(Qt::AltModifier, VK_OEM_3, 0x29)};
    defaults[HotkeyAction::CycleForward]            = {makePhysicalBinding(Qt::NoModifier, VK_TAB, 0x0F)};
    defaults[HotkeyAction::CycleBackward]           = {makePhysicalBinding(Qt::ShiftModifier, VK_TAB, 0x0F)};
    defaults[HotkeyAction::MoveSelectionUp]         = {};
    defaults[HotkeyAction::MoveSelectionDown]       = {};
    defaults[HotkeyAction::ActivateSelected]        = {makePhysicalBinding(Qt::NoModifier, VK_RETURN, 0x1C)};
    defaults[HotkeyAction::DismissSwitcher]         = {makePhysicalBinding(Qt::NoModifier, VK_ESCAPE, 0x01)};
    defaults[HotkeyAction::TogglePause]             = {};
    defaults[HotkeyAction::SwitchToPreviousWindow]  = {
        makePhysicalBinding(Qt::AltModifier | Qt::ShiftModifier, VK_TAB, 0x0F)
    };
    defaults[HotkeyAction::ShowSwitcherStayOpen]    = {};
    return defaults;
}

inline QList<HotkeyBinding> requiredBindingsForAction(HotkeyAction action) {
    switch (action) {
        case HotkeyAction::DismissSwitcher:
            return {makePhysicalBinding(Qt::NoModifier, VK_ESCAPE, 0x01)};
        default:
            return {};
    }
}

inline bool isProtectedBinding(HotkeyAction action, const HotkeyBinding& binding) {
    auto required = requiredBindingsForAction(action);
    for (const auto& rb : required)
        if (rb == binding) return true;
    return false;
}

inline void ensureRequiredBindings(HotkeyAction action, QList<HotkeyBinding>& bindings) {
    auto required = requiredBindingsForAction(action);
    for (const auto& rb : required) {
        bool found = false;
        for (const auto& b : bindings)
            if (b == rb) { found = true; break; }
        if (!found)
            bindings.prepend(rb);
    }
}

void normalizeHotkeyBindings(HotkeyBindings& bindings);

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
        b.modifiers = HotkeyStrings::stringToModifiers(str.left(plusIdx));
        b.vkCode = HotkeyStrings::stringToVkCode(str.mid(plusIdx + 1));
    } else {
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

#endif //WIN_SWITCHER_HOTKEYBINDING_H
