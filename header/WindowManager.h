#ifndef WIN_SWITCHER_WINDOWMANAGER_H
#define WIN_SWITCHER_WINDOWMANAGER_H

#include <QObject>
#include <QHash>
#include <QList>
#include <QMap>
#include "WindowTypes.h"

class WindowManager : public QObject {
    Q_OBJECT

public:
    explicit WindowManager(HWND selfHwnd = nullptr, QObject* parent = nullptr);
    void setSelfHwnd(HWND hwnd);

    QList<WindowGroup> prepareWindowGroupList();
    QList<HWND> buildGroupWindowOrder(const QString& exePath) const;

    static HWND rotateWindowInGroup(const QList<HWND>& windows, HWND current, bool forward = true);
    static HWND rotateNormalWindowInGroup(const QList<HWND>& windows, HWND current, bool forward = true);

    QList<HWND>& groupWindowOrder();
    void clearGroupWindowOrder();
    void recordWindowActivation(HWND hwnd);

private:
    HWND m_selfHwnd = nullptr;
    QList<HWND> m_groupWindowOrder;
    QMap<HWND, qint64> m_windowActivationTimes;
};

#endif //WIN_SWITCHER_WINDOWMANAGER_H
