#ifndef WIN_SWITCHER_ACTIVATIONHISTORY_H
#define WIN_SWITCHER_ACTIVATIONHISTORY_H

#include <QMap>
#include <QString>
#include "WindowTypes.h"

struct ActivationHistory {
    void record(const AppIdentity& identity);
    qint64 lastActivationTime(const AppIdentity& identity) const;
    void clear();

    const QMap<QString, qint64>& times() const { return m_times; }

private:
    QMap<QString, qint64> m_times;
};

#endif //WIN_SWITCHER_ACTIVATIONHISTORY_H