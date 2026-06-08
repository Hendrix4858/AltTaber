#ifndef WIN_SWITCHER_TASKBARWHEELHOOKER_H
#define WIN_SWITCHER_TASKBARWHEELHOOKER_H

#include <QObject>
#include <Windows.h>

class TaskbarWheelHooker : public QObject {
    Q_OBJECT

public:
    TaskbarWheelHooker();
    ~TaskbarWheelHooker() override;
    void setPaused(bool paused);

signals:
    void tabWheelEvent(const QString& exePath, bool isUp, int windows);
    void leaveTaskbar();

private:
    HHOOK h_mouse = nullptr;
    bool m_paused = false;

    friend LRESULT CALLBACK mouseProc(int nCode, WPARAM wParam, LPARAM lParam);
};


#endif //WIN_SWITCHER_TASKBARWHEELHOOKER_H
