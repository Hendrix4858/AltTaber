#ifndef WIN_SWITCHER_OVERLAYCONTROLLER_H
#define WIN_SWITCHER_OVERLAYCONTROLLER_H

#include <QObject>
#include <QScreen>
#include <Windows.h>
#include "core/HotkeyAction.h"

class OverlayView;
class WindowGroupModel;
class WindowManager;

enum class OverlayIntent {
    ShowSwitcher,
    Dismiss,
    FallbackShow,
    SessionEndConditionMet,
    ShowSwitcherBackward,
};

struct OverlaySessionInfo {
    HotkeyAction triggeringAction = HotkeyAction::SwitchToNextWindow;
    SessionEndTrigger endTrigger = SessionEndTrigger::ModifierRelease;
};

class OverlayController : public QObject {
    Q_OBJECT

public:
    enum class OverlayState {
        Hidden,
        Showing,
        Visible,
        Hiding,
    };

    explicit OverlayController(OverlayView& view, WindowGroupModel* model, WindowManager* wm,
                               QObject* parent = nullptr);

    // ── Single external entry point for all state changes ──
    Q_INVOKABLE void handleIntent(OverlayIntent intent);

    // ── State-aware global action handler ──
    void handleGlobalAction(HotkeyAction action, Qt::KeyboardModifiers modifiers);

    bool isVisible() const;
    bool isForeground() const;
    OverlayState overlayState() const { return m_overlayState; }
    bool stayOpenMode() const { return m_stayOpenMode; }
    void setStayOpenMode(bool v) { m_stayOpenMode = v; }

    QRect calculateGeometry(QScreen* screen = nullptr);
    int calculateInitialIndex() const;

    void setOverlayBindings(const HotkeyBindings& bindings);
    const HotkeyBindings& overlayBindings() const { return m_overlayBindings; }

    void setSessionInfo(const OverlaySessionInfo& info) { m_sessionInfo = info; }

    void notifyForegroundChanged(HWND hwnd);
    void warmupCache();

signals:
    void sessionFinished();
    void showRequested();
    void hideRequested();
    void stateChanged(OverlayController::OverlayState state);
    void actionForwarded(HotkeyAction action, Qt::KeyboardModifiers modifiers);

private:
    // ── State machine ──
    void reconcileState();
    void transition(OverlayIntent intent);

    // ── Action primitives (only called from transition) ──
    void showWindow();
    void hideWindow();
    bool refreshWindowList();
    bool prepareListWidget();

    bool forceShow();

    OverlayView& m_view;
    WindowGroupModel* m_model;
    WindowManager* m_windowManager;

    OverlayState m_overlayState = OverlayState::Hidden;
    bool m_stayOpenMode = false;
    bool m_listDirty = true;
    bool m_wasInvokedBackward = false;
    OverlaySessionInfo m_sessionInfo;

    HotkeyBindings m_overlayBindings;
};

#endif //WIN_SWITCHER_OVERLAYCONTROLLER_H
