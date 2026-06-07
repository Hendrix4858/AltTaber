#ifndef WIN_SWITCHER_WIDGET_H
#define WIN_SWITCHER_WIDGET_H

#include <QWidget>
#include <Windows.h>
#include <QListView>
#include <QDebug>
#include <QTimer>
#include "utils/HotkeyAction.h"

class WindowGroupModel;
class WindowManager;
struct WindowGroup;

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
    enum class OverlayState {
        Hidden,
        Showing,
        Visible,
        Hiding,
    };

    enum ForegroundChangeSource {
        WinEvent,
        Inner,
    };

    Q_ENUM(ForegroundChangeSource) // for QMetaEnum, to QString

public:
    explicit Widget(WindowManager* wm, QWidget* parent = nullptr);
    bool prepareListWidget();
    void warmupCache();
    Q_INVOKABLE bool requestShow();
    void notifyForegroundChanged(HWND hwnd);

    HWND hWnd() { return (HWND) winId(); }

    bool isForeground() { return GetForegroundWindow() == hWnd(); }

    OverlayState overlayState() const { return m_overlayState; }

    ~Widget() override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void rotateTaskbarWindowInGroup(const QString& exePath, bool forward, int windows);
    void clearGroupWindowOrder();
    void handleListItemClicked(const QModelIndex& index);
    WindowManager* windowManager() const { return m_wm; }

    // Hotkey dispatch entry points
    void handleOverlayAction(HotkeyAction action, Qt::KeyboardModifiers modifiers);
    void handleGlobalAction(HotkeyAction action, Qt::KeyboardModifiers modifiers);

    void updateOverlayBindings(const HotkeyBindings& bindings);

private:
    bool forceShow();
    void showLabelForItem(const QModelIndex& index, QString text = QString());
    void setupLabelFont();

    void enterGroupWindowMode();
    void exitGroupWindowMode(bool activateSelected);
    void activateCurrentAndHide();
    void recalculateGeometry(QScreen* screen = nullptr);

    // Lazy icon loading
    void lazyLoadIcons();
    void loadNextIcon(int generation);

private:
    Ui::Widget* ui;
    QListView* lv = nullptr;
    WindowGroupModel* m_model = nullptr;
    WindowManager* m_wm = nullptr;
    const QMargins ListWidgetMargin{24, 24, 24, 24};

    // eventFilter state (replaces static locals)
    int lastWheelRow = -1;
    HWND lastWheelHwnd = nullptr;
    bool lastWheelIsRollUp = true;

    // Letter jump state
    QChar m_jumpLastLetter;
    int m_jumpLastIndex = -1;

    // Alt+` window focus mode state
    bool m_isInGroupWindowMode = false;
    QList<WindowGroup> m_backupGroupList;
    int m_backupGroupIndex = 0;

    // Stay-open mode (keep overlay visible after Alt release)
    bool m_stayOpenMode = false;

    // Lazy icon loading state
    int m_iconLoadIndex = 0;
    int m_iconLoadGeneration = 0;
    bool m_iconsFullyLoaded = false;


    // rotateTaskbarWindowInGroup state (replaces static locals)
    QString lastTaskbarPath;
    HWND lastTaskbarHwnd = nullptr;
    bool lastTaskbarForward = true;
    QTimer* taskbarTimer = nullptr;

    // Overlay hotkey bindings
    HotkeyBindings m_overlayBindings;

    // Overlay state machine
    OverlayState m_overlayState = OverlayState::Hidden;
};


#endif //WIN_SWITCHER_WIDGET_H
