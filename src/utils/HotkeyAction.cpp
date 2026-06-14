#include "utils/HotkeyAction.h"

void normalizeHotkeyBindings(HotkeyBindings& bindings) {
    auto defaults = defaultHotkeyBindings();
    for (auto action : AllActions) {
        if (!bindings.contains(action)) {
            if (defaults.contains(action) && !defaults[action].isEmpty())
                bindings[action] = defaults[action];
        }
        ensureRequiredBindings(action, bindings[action]);
    }
}
