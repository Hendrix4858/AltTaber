#ifndef WIN_SWITCHER_WIDGET_H
#define WIN_SWITCHER_WIDGET_H

#include <QWidget>
#include <Windows.h>
#include <QListView>

#include "utils/HotkeyAction.h"
#include "OverlayController.h"

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

class Widget : public QWidget {
    Q_OBJECT

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void hideEvent(QHideEvent* event) override;

public:
    using OverlayState = OverlayController::OverlayState;

    explicit Widget(WindowManager* wm, QWidget* parent = nullptr);
    void warmupCache();
    Q_INVOKABLE bool requestShow(OverlayIntent why = OverlayIntent::ShowSwitcher);
    void hideOverlay();
    void notifyForegroundChanged(HWND hwnd);
    OverlayController* overlayController() const { return m_overlayCtrl; }

    HWND hWnd() { return (HWND) winId(); }
    bool isForeground() { return GetForegroundWindow() == hWnd(); }

    OverlayState overlayState() const;
    ~Widget() override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void handleListItemClicked(const QModelIndex& index);
    WindowManager* windowManager() const { return m_wm; }
    TaskbarWindowCycler* taskbarCycler() const { return m_taskbarCycler; }

    Q_INVOKABLE void handleOverlayAction(HotkeyAction action, Qt::KeyboardModifiers modifiers);
    Q_INVOKABLE void handleGlobalAction(HotkeyAction action, Qt::KeyboardModifiers modifiers);
    void onActivationModifiersReleased();

    void updateOverlayBindings(const HotkeyBindings& bindings);

signals:
    void overlayDismissed();
    void overlayShown();

private:
    void setupLabelFont();
    void activateCurrentGroupWindow();

    static void activateWindowWithVerification(HWND targetHwnd, const QString& exePath,
                                               const QString& title);

    Ui::Widget* ui;
    QListView* lv = nullptr;
    WindowGroupModel* m_model = nullptr;
    WindowManager* m_wm = nullptr;
    const QMargins ListWidgetMargin{24, 24, 24, 24};

    OverlayController* m_overlayCtrl;
    SelectionController* m_selectCtrl;
    TaskbarWindowCycler* m_taskbarCycler;
    GroupWindowCycler* m_cyc;


};

#endif //WIN_SWITCHER_WIDGET_H
