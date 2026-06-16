#ifndef WIN_SWITCHER_HOTKEYCONFLICTRESOLVER_H
#define WIN_SWITCHER_HOTKEYCONFLICTRESOLVER_H

#include <QList>
#include "core/HotkeyAction.h"

class HotkeyConflictResolver {
public:
    struct ConflictResult {
        HotkeyAction action = HotkeyAction::SwitchToNextWindow;
        int index = -1;
        bool found = false;
    };

    static ConflictResult findConflict(HotkeyAction action, const HotkeyBinding& binding,
                                       const HotkeyBindings& allBindings);

    static bool hasSingleLetterBinding(const HotkeyBindings& allBindings);
};

#endif //WIN_SWITCHER_HOTKEYCONFLICTRESOLVER_H
