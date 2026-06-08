#ifndef WIN_SWITCHER_WINDOWGROUPER_H
#define WIN_SWITCHER_WINDOWGROUPER_H

#include <QList>
#include "WindowTypes.h"

struct ActivationHistory;

namespace WindowGrouper {
    QList<WindowGroup> groupWindows(const QList<WindowDescriptor>& windows,
                                    const ActivationHistory* history = nullptr,
                                    HWND selfHwnd = nullptr);
}

#endif //WIN_SWITCHER_WINDOWGROUPER_H
