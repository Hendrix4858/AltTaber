#include "WindowManager.h"
#include "WindowEnumerator.h"
#include "WindowGrouper.h"

WindowManager::WindowManager(HWND selfHwnd, QObject* parent)
    : QObject(parent), m_selfHwnd(selfHwnd) {}

void WindowManager::setSelfHwnd(HWND hwnd) { m_selfHwnd = hwnd; }

QList<WindowGroup> WindowManager::prepareWindowGroupList() {
    return WindowGrouper::groupWindows(
        WindowEnumerator::enumValidWindows(),
        &m_activationHistory,
        m_selfHwnd);
}

void WindowManager::recordWindowActivation(HWND hwnd) {
    m_activationHistory.record(hwnd);
}
