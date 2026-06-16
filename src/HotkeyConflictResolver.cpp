#include "HotkeyConflictResolver.h"

HotkeyConflictResolver::ConflictResult
HotkeyConflictResolver::findConflict(HotkeyAction action, const HotkeyBinding& binding,
                                     const HotkeyBindings& allBindings) {
    ConflictResult result;
    auto scope = hotkeyActionScope(action);
    for (auto it = allBindings.begin(); it != allBindings.end(); ++it) {
        if (it.key() == action) continue;
        if (hotkeyActionScope(it.key()) != scope) continue;
        for (int i = 0; i < it.value().size(); ++i) {
            if (it.value()[i] == binding) {
                result.action = it.key();
                result.index = i;
                result.found = true;
                return result;
            }
        }
    }
    return result;
}

bool HotkeyConflictResolver::hasSingleLetterBinding(const HotkeyBindings& allBindings) {
    for (auto it = allBindings.begin(); it != allBindings.end(); ++it) {
        for (const auto& b : it.value()) {
            if (b.isSingleLetter())
                return true;
        }
    }
    return false;
}
