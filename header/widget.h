#ifndef WIN_SWITCHER_WIDGET_H
#define WIN_SWITCHER_WIDGET_H

#include <QWidget>
#include <Windows.h>
#include <QListView>

#include "core/HotkeyAction.h"
#include "OverlayController.h"
#include "OverlayView.h"

class WindowGroupModel;
class WindowManager;
class SelectionController;
class TaskbarWindowCycler;
class GroupWindowCycler;

QT_BEGIN_NAMESPACE
namespace Ui {
    class Widget;
}
QT_END_NAMESPACE

class Widget : public QWidget, public OverlayView {
    Q_OBJECT

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

public:
    explicit Widget(WindowManager* wm, QWidget* parent = nullptr);
    ~Widget() override;

    // ── OverlayView interface ──
    void showOverlay() override;
    void doHide() override;
    void updateGroups(const QList<WindowGroup>& groups) override;
    void applyIconLayout(int iconSize, int gridWidth, int gridHeight) override;
    void applyLayout(const OverlayLayout& layout) override;
    void setCurrentIndex(int index) override;
    HWND hWnd() const override;

    void warmupCache();
    Q_INVOKABLE bool requestShow(OverlayIntent why = OverlayIntent::ShowSwitcher,
                                  HotkeyAction triggeringAction = HotkeyAction::SwitchToNextWindow);
    void hideOverlay();
    void notifyForegroundChanged(HWND hwnd);
    OverlayController* overlayController() const { return m_overlayCtrl; }

    bool isForeground() { return GetForegroundWindow() == (HWND) winId(); }
    bool eventFilter(QObject* watched, QEvent* event) override;
    void handleListItemClicked(const QModelIndex& index);

    WindowManager* windowManager() const { return m_windowManager; }
    TaskbarWindowCycler* taskbarCycler() const { return m_taskbarCycler; }
    SelectionController* selectionController() const { return m_selectCtrl; }
    GroupWindowCycler* groupWindowCycler() const { return m_groupCycler; }

    Q_INVOKABLE void handleOverlayAction(HotkeyAction action, Qt::KeyboardModifiers modifiers);
    Q_INVOKABLE void handleHookOverlayAction(HotkeyAction action, Qt::KeyboardModifiers modifiers);
    void onActivationModifiersReleased();
    void updateOverlayBindings(const HotkeyBindings& bindings);

signals:
    void overlayDismissed();
    void overlayShown();

private:
    void setupLabelFont();
    void activateCurrentGroupWindow();
    void applyWindowEffects();

    static void activateWindowWithVerification(HWND targetHwnd, const QString& exePath,
                                                const QString& title);

    Ui::Widget* ui;
    QListView* m_listView = nullptr;
    WindowGroupModel* m_model = nullptr;
    WindowManager* m_windowManager = nullptr;
    const QMargins m_listWidgetMargin{24, 24, 24, 24};

    OverlayController* m_overlayCtrl;
    SelectionController* m_selectCtrl;
    TaskbarWindowCycler* m_taskbarCycler;
    GroupWindowCycler* m_groupCycler;
};

#endif //WIN_SWITCHER_WIDGET_H
