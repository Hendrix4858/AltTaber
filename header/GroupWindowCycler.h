#ifndef WIN_SWITCHER_GROUPWINDOWCYCLER_H
#define WIN_SWITCHER_GROUPWINDOWCYCLER_H

#include <QObject>
#include "WindowCycleOrder.h"

class GroupWindowCycler : public QObject {
    Q_OBJECT

public:
    explicit GroupWindowCycler(QObject* parent = nullptr);

    QList<HWND> buildGroupWindowOrder(const QString& exePath);
    QList<HWND>& groupWindowOrder();
    void clearGroupWindowOrder();

    static HWND rotateWindowInGroup(const QList<HWND>& windows, HWND current, bool forward = true);
    static HWND rotateNormalWindowInGroup(const QList<HWND>& windows, HWND current, bool forward = true);

private:
    WindowCycleOrder m_cycleOrder;
};

#endif //WIN_SWITCHER_GROUPWINDOWCYCLER_H
