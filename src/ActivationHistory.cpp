#include "ActivationHistory.h"
#include <QDateTime>

void ActivationHistory::record(const AppIdentity& identity) {
    m_times[identity.groupKey()] = QDateTime::currentMSecsSinceEpoch();
}

qint64 ActivationHistory::lastActivationTime(const AppIdentity& identity) const {
    auto it = m_times.find(identity.groupKey());
    return it != m_times.end() ? it.value() : 0;
}

void ActivationHistory::clear() {
    m_times.clear();
}
