#include "GroupWindowCycler.h"

GroupWindowCycler::GroupWindowCycler(QObject* parent)
    : QObject(parent) {}

QList<HWND>& GroupWindowCycler::groupWindowOrder() {
    return m_cycleOrder.order();
}

void GroupWindowCycler::clearGroupWindowOrder() {
    m_cycleOrder.clear();
}

HWND GroupWindowCycler::rotateWindowInGroup(const QList<HWND>& windows, HWND current, bool forward) {
    return WindowCycleOrder::rotate(windows, current, forward);
}

HWND GroupWindowCycler::rotateNormalWindowInGroup(const QList<HWND>& windows, HWND current, bool forward) {
    return WindowCycleOrder::rotateNormal(windows, current, forward);
}
