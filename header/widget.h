#ifndef WIN_SWITCHER_WIDGET_H
#define WIN_SWITCHER_WIDGET_H

#include <QWidget>
#include <Windows.h>
#include <QListView>
#include <QDebug>
#include <QTimer>

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

public:
    enum ForegroundChangeSource {
        WinEvent,
        Inner,
    };

    Q_ENUM(ForegroundChangeSource) // for QMetaEnum, to QString

public:
    explicit Widget(WindowManager* wm, QWidget* parent = nullptr);
    bool prepareListWidget();
    Q_INVOKABLE bool requestShow();
    void notifyForegroundChanged(HWND hwnd);

    HWND hWnd() { return (HWND) winId(); }

    bool isForeground() { return GetForegroundWindow() == hWnd(); }

    ~Widget() override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void rotateTaskbarWindowInGroup(const QString& exePath, bool forward, int windows);
    void clearGroupWindowOrder();
    WindowManager* windowManager() const { return m_wm; }

private:
    bool forceShow();
    void showLabelForItem(const QModelIndex& index, QString text = QString());
    void setupLabelFont();

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

    // rotateTaskbarWindowInGroup state (replaces static locals)
    QString lastTaskbarPath;
    HWND lastTaskbarHwnd = nullptr;
    bool lastTaskbarForward = true;
    QTimer* taskbarTimer = nullptr;
};


#endif //WIN_SWITCHER_WIDGET_H
