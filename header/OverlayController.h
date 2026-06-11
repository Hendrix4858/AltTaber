#ifndef WIN_SWITCHER_OVERLAYCONTROLLER_H
#define WIN_SWITCHER_OVERLAYCONTROLLER_H

#include <QObject>
#include <QWidget>
#include <QScreen>
#include <Windows.h>
#include "utils/HotkeyAction.h"

class WindowGroupModel;
class WindowManager;

enum class OverlayIntent {
    ShowSwitcher,    // Alt+Tab pressed — show overlay from Hidden
    AltReleased,     // Alt key released — activate selected + hide
    Dismiss,         // Escape / app inactive / foreground lost
    FallbackShow,    // System Alt+Tab detected (ForegroundStaging) — takeover
    ActivationModifiersReleased,  // All activation modifiers released — activate + hide
    ShowSwitcherBackward,  // Alt+Shift+Tab pressed — show overlay, select last group
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

    explicit OverlayController(QWidget* widget, QWidget* listView, QWidget* labelWidget,
                               WindowGroupModel* model, WindowManager* wm,
                               QObject* parent = nullptr);

    // ── Single external entry point for all state changes ──
    Q_INVOKABLE void handleIntent(OverlayIntent intent);

    bool isVisible() const;
    bool isForeground() const;
    OverlayState overlayState() const { return m_overlayState; }
    bool stayOpenMode() const { return m_stayOpenMode; }
    void setStayOpenMode(bool v) { m_stayOpenMode = v; }

    void recalculateGeometry(QScreen* screen = nullptr);

    void setOverlayBindings(const HotkeyBindings& bindings);
    const HotkeyBindings& overlayBindings() const { return m_overlayBindings; }

    void notifyForegroundChanged(HWND hwnd);
    void warmupCache();

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

    QWidget* m_widget;
    QWidget* m_lv;
    QWidget* m_label;
    WindowGroupModel* m_model;
    WindowManager* m_wm;

    const QMargins m_margin{24, 24, 24, 24};

    OverlayState m_overlayState = OverlayState::Hidden;
    bool m_stayOpenMode = false;
    bool m_listDirty = true;
    bool m_selectBackward = false;

    HotkeyBindings m_overlayBindings;
};

#endif //WIN_SWITCHER_OVERLAYCONTROLLER_H
