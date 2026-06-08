#include "ActivationHistory.h"
#include <QDateTime>

void ActivationHistory::record(HWND hwnd) {
    m_times[hwnd] = QDateTime::currentMSecsSinceEpoch();
}

qint64 ActivationHistory::latestActivation(HWND hwnd) const {
    auto it = m_times.find(hwnd);
    return it != m_times.end() ? it.value() : 0;
}

void ActivationHistory::clear() {
    m_times.clear();
}
