#ifndef WIN_SWITCHER_ACTIONROUTER_H
#define WIN_SWITCHER_ACTIONROUTER_H

#include <QObject>
#include "core/HotkeyAction.h"
#include "OverlayController.h"

class SelectionController;
class GroupWindowCycler;
class WindowManager;

class ActionRouter : public QObject {
    Q_OBJECT

public:
    explicit ActionRouter(QObject* parent = nullptr);

    void setOverlayController(OverlayController* ctrl);
    void setSelectionController(SelectionController* selCtrl);
    void setGroupWindowCycler(GroupWindowCycler* cycler);
    void setWindowManager(WindowManager* wm);

public slots:
    void routeGlobalAction(HotkeyAction action, Qt::KeyboardModifiers modifiers);

private:
    OverlayController* m_overlayCtrl = nullptr;
    SelectionController* m_selectCtrl = nullptr;
    GroupWindowCycler* m_groupCycler = nullptr;
    WindowManager* m_windowManager = nullptr;
};

#endif //WIN_SWITCHER_ACTIONROUTER_H
