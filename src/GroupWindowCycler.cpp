#include "GroupWindowCycler.h"

GroupWindowCycler::GroupWindowCycler(QObject* parent)
    : QObject(parent) {}

QList<HWND>& GroupWindowCycler::groupWindowOrder() {
    return m_cycleOrder.order();
}

void GroupWindowCycler::clearGroupWindowOrder() {
    m_cycleOrder.clear();
}

HWND GroupWindowCycler::rotateWindow(const QList<HWND>& windows, HWND current, bool forward) {
    return WindowCycleOrder::rotate(windows, current, forward);
}

HWND GroupWindowCycler::rotateToNormalWindow(const QList<HWND>& windows, HWND current, bool forward) {
    return WindowCycleOrder::rotateNormal(windows, current, forward);
}
