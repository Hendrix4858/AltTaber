#ifndef WIN_SWITCHER_WHEELEVENTPROCESSOR_H
#define WIN_SWITCHER_WHEELEVENTPROCESSOR_H

#include <Windows.h>
#include <QObject>

class QWheelEvent;
class QListView;
class WindowGroupModel;
class WindowManager;
class GroupWindowCycler;

class WheelEventProcessor : public QObject {
    Q_OBJECT

public:
    explicit WheelEventProcessor(QObject* parent = nullptr);

    bool handleWheelEvent(QWheelEvent* event, QListView* listView,
                          WindowGroupModel* model, WindowManager* wm,
                          GroupWindowCycler* cyc);

    void handleKeyboardNavigation(QListView* listView,
                                  WindowGroupModel* model, WindowManager* wm,
                                  GroupWindowCycler* cyc, bool forward);

    void reset();

signals:
    void foregroundChanged(HWND hwnd);
    void labelTextChanged(const QString& text);
    void aboutToActivateWindow();

private:
    int m_lastRow = -1;
    HWND m_lastHwnd = nullptr;
    bool m_lastScrollDirection = true;
};

#endif //WIN_SWITCHER_WHEELEVENTPROCESSOR_H
