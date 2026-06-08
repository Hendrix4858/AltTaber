#ifndef WIN_SWITCHER_ACTIVATIONHISTORY_H
#define WIN_SWITCHER_ACTIVATIONHISTORY_H

#include <Windows.h>
#include <QMap>

struct ActivationHistory {
    void record(HWND hwnd);
    qint64 latestActivation(HWND hwnd) const;
    void clear();

    const QMap<HWND, qint64>& times() const { return m_times; }

private:
    QMap<HWND, qint64> m_times;
};

#endif //WIN_SWITCHER_ACTIVATIONHISTORY_H
