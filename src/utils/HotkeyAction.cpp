#include "utils/HotkeyAction.h"

void normalizeHotkeyBindings(HotkeyBindings& bindings) {
    auto defaults = defaultHotkeyBindings();
    for (auto action : AllActions) {
        if (!bindings.contains(action) || bindings[action].isEmpty()) {
            if (defaults.contains(action) && !defaults[action].isEmpty())
                bindings[action] = defaults[action];
        }
    }
    auto& showBinds = bindings[HotkeyAction::ShowSwitcher];
    bool hasAltTab = false;
    for (const auto& b : showBinds) {
        if (b.vkCode == VK_TAB && b.modifiers == Qt::AltModifier) {
            hasAltTab = true;
            break;
        }
    }
    if (!hasAltTab)
        showBinds.append(HotkeyBinding::fromString("Alt+Tab"));
}
