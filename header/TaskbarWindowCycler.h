#ifndef WIN_SWITCHER_TASKBARWINDOWCYCLER_H
#define WIN_SWITCHER_TASKBARWINDOWCYCLER_H

#include <QObject>
#include <QTimer>
#include <Windows.h>

class GroupWindowCycler;
class WindowManager;

class TaskbarWindowCycler : public QObject {
    Q_OBJECT

public:
    explicit TaskbarWindowCycler(GroupWindowCycler* cyc, WindowManager* wm,
                                 QObject* parent = nullptr);

public slots:
    void rotate(const QString& exePath, bool forward, int windowCount);
    void clearOrder();

private:
    GroupWindowCycler* m_groupCycler;
    WindowManager* m_windowManager;

    QString m_lastTaskbarExePath;
    HWND m_lastTaskbarHwnd = nullptr;
    bool m_lastTaskbarDirection = true;
    QTimer* m_releaseTimer = nullptr;
};

#endif //WIN_SWITCHER_TASKBARWINDOWCYCLER_H
