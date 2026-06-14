#ifndef WIN_SWITCHER_WHEELEVENTPROCESSOR_H
#define WIN_SWITCHER_WHEELEVENTPROCESSOR_H

#include <QObject>
#include <Windows.h>

class QWheelEvent;
class QListView;
class WindowGroupModel;
class WindowManager;
class GroupWindowCycler;

class WheelEventProcessor : public QObject {
    Q_OBJECT

public:
    explicit WheelEventProcessor(QObject* parent = nullptr);

    bool handleWheelEvent(QWheelEvent* event, QListView* lv,
                          WindowGroupModel* model, WindowManager* wm,
                          GroupWindowCycler* cyc);

    void reset();

signals:
    void foregroundChanged(HWND hwnd);
    void labelTextChanged(const QString& text);

private:
    int m_lastRow = -1;
    HWND m_lastHwnd = nullptr;
    bool m_lastIsRollUp = true;
};

#endif //WIN_SWITCHER_WHEELEVENTPROCESSOR_H
