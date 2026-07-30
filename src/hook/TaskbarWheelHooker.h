#ifndef WIN_SWITCHER_TASKBARWHEELHOOKER_H
#define WIN_SWITCHER_TASKBARWHEELHOOKER_H

#include <Windows.h>
#include <QObject>
#include <QTimer>

class TaskbarWheelHooker : public QObject {
    Q_OBJECT

public:
    TaskbarWheelHooker();
    ~TaskbarWheelHooker() override;
    void setPaused(bool paused);

signals:
    void tabWheelEvent(const QString& exePath, bool isUp, int windows, const QString& appid);
    void leaveTaskbar();

private slots:
    void onDebounceTimeout();

private:
    HHOOK m_mouseHook = nullptr;
    bool m_paused = false;
    int m_accumulatedDelta = 0;
    QTimer* m_debounceTimer = nullptr;
    bool m_onTaskbar = false;

    friend LRESULT CALLBACK mouseProc(int nCode, WPARAM wParam, LPARAM lParam);
};

#endif
