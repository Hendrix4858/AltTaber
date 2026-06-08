#ifndef WIN_SWITCHER_TASKBARWINDOWCYCLER_H
#define WIN_SWITCHER_TASKBARWINDOWCYCLER_H

#include <QObject>
#include <QTimer>
#include <Windows.h>

class GroupWindowCycler;

class TaskbarWindowCycler : public QObject {
    Q_OBJECT

public:
    explicit TaskbarWindowCycler(GroupWindowCycler* cyc, QObject* parent = nullptr);

public slots:
    void rotate(const QString& exePath, bool forward, int windows);
    void clearOrder();

private:
    GroupWindowCycler* m_cyc;

    QString lastTaskbarPath;
    HWND lastTaskbarHwnd = nullptr;
    bool lastTaskbarForward = true;
    QTimer* taskbarTimer = nullptr;
};

#endif //WIN_SWITCHER_TASKBARWINDOWCYCLER_H
