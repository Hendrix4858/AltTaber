#ifndef WIN_SWITCHER_WINDOWCYCLEORDER_H
#define WIN_SWITCHER_WINDOWCYCLEORDER_H

#include <Windows.h>
#include <QList>
#include <QString>
#include "WindowEnumerator.h"

class WindowCycleOrder {
public:
    QList<HWND> build(const QString& exePath);

    QList<HWND>& order() { return m_order; }
    const QList<HWND>& order() const { return m_order; }

    void clear();
    bool isEmpty() const { return m_order.isEmpty(); }

    static HWND rotate(const QList<HWND>& windows, HWND current, bool forward = true);
    static HWND rotateNormal(const QList<HWND>& windows, HWND current, bool forward = true);

    void setFilterRules(const WindowFilterRules* rules) { m_filterRules = rules; }

private:
    QList<HWND> m_order;
    const WindowFilterRules* m_filterRules = nullptr;
};

#endif //WIN_SWITCHER_WINDOWCYCLEORDER_H
