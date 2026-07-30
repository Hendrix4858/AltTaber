#ifndef WIN_SWITCHER_TASKBARWINDOWCYCLER_H
#define WIN_SWITCHER_TASKBARWINDOWCYCLER_H

#include <QObject>
#include <QTimer>
#include <QList>
#include <Windows.h>

class GroupWindowCycler;
class WindowManager;

class TaskbarWindowCycler : public QObject {
    Q_OBJECT

public:
    explicit TaskbarWindowCycler(GroupWindowCycler* cyc, WindowManager* wm,
                                 QObject* parent = nullptr);

public slots:
    void rotate(const QString& exePath, bool forward, int windowCount, const QString& appid);
    void clearOrder();

private:
    void invalidateCache();

    struct WindowCache {
        QString exePath;
        QList<HWND> hwnds;
        qint64 timestamp = 0;
    };

    GroupWindowCycler* m_groupCycler;
    WindowManager* m_windowManager;

    QString m_lastTaskbarExePath;
    QString m_lastTaskbarAppid;
    HWND m_lastTaskbarHwnd = nullptr;
    bool m_lastTaskbarDirection = true;
    QTimer* m_releaseTimer = nullptr;
    WindowCache m_windowCache;
};

#endif //WIN_SWITCHER_TASKBARWINDOWCYCLER_H
