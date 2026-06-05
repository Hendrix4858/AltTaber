#ifndef WIN_SWITCHER_TASKBARWHEELHOOKER_H
#define WIN_SWITCHER_TASKBARWHEELHOOKER_H

#include <QObject>
#include <Windows.h>

class TaskbarWheelHooker : public QObject {
    Q_OBJECT

public:
    TaskbarWheelHooker();
    ~TaskbarWheelHooker() override;

signals:
    void tabWheelEvent(const QString& exePath, bool isUp, int windows);
    void leaveTaskbar();

private:
    HHOOK h_mouse = nullptr;
};


#endif //WIN_SWITCHER_TASKBARWHEELHOOKER_H
