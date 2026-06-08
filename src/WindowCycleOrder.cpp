#include "WindowCycleOrder.h"

HWND WindowCycleOrder::rotate(const QList<HWND>& windows, HWND current, bool forward) {
    const auto N = windows.size();
    if (N == 1) return windows.first();
    for (int i = 0; i < N; i++) {
        if (windows.at(i) == current) {
            auto next_i = forward ? (i + 1) : (i - 1);
            return windows.at((next_i + N) % N);
        }
    }
    return nullptr;
}

HWND WindowCycleOrder::rotateNormal(const QList<HWND>& windows, HWND current, bool forward) {
    for (int i = 0; IsIconic(current) && i < windows.size(); i++)
        current = rotate(windows, current, forward);
    return IsIconic(current) ? nullptr : current;
}
