#ifndef WIN_SWITCHER_WINDOWCYCLEORDER_H
#define WIN_SWITCHER_WINDOWCYCLEORDER_H

#include <Windows.h>
#include <QList>

class WindowCycleOrder {
public:
    QList<HWND>& order() { return m_order; }
    const QList<HWND>& order() const { return m_order; }

    void clear() { m_order.clear(); }
    bool isEmpty() const { return m_order.isEmpty(); }

    static HWND rotate(const QList<HWND>& windows, HWND current, bool forward = true);
    static HWND rotateNormal(const QList<HWND>& windows, HWND current, bool forward = true);

private:
    QList<HWND> m_order;
};

#endif //WIN_SWITCHER_WINDOWCYCLEORDER_H
