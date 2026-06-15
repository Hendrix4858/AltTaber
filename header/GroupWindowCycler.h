#ifndef WIN_SWITCHER_GROUPWINDOWCYCLER_H
#define WIN_SWITCHER_GROUPWINDOWCYCLER_H

#include <QObject>
#include "WindowCycleOrder.h"

class GroupWindowCycler : public QObject {
    Q_OBJECT

public:
    explicit GroupWindowCycler(QObject* parent = nullptr);

    QList<HWND>& groupWindowOrder();
    void clearGroupWindowOrder();

    static HWND rotateWindow(const QList<HWND>& windows, HWND current, bool forward = true);
    static HWND rotateToNormalWindow(const QList<HWND>& windows, HWND current, bool forward = true);

private:
    WindowCycleOrder m_cycleOrder;
};

#endif //WIN_SWITCHER_GROUPWINDOWCYCLER_H
