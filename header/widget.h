#ifndef WIN_SWITCHER_WIDGET_H
#define WIN_SWITCHER_WIDGET_H

#include <QWidget>
#include <Windows.h>
#include <QListView>
#include <QDebug>
#include <QTimer>

class WindowGroupModel;

struct WindowGroup;

struct WindowInfo {
    QString title;
    QString className;
    HWND hwnd = nullptr;
};

// C++17 inline: 防止重定义
inline QDebug operator<<(QDebug dbg, const WindowInfo& info) {
    dbg.nospace() << "WindowInfo(" << info.title << ", " << info.className << ", " << info.hwnd << ")";
    return dbg.space();
}

Q_DECLARE_METATYPE(WindowInfo) // for QVariant
struct WindowGroup {
    WindowGroup() = default;

    void addWindow(const WindowInfo& window) {
        windows.append(window);
    }

    QString exePath;
    QIcon icon;
    QList<WindowInfo> windows;
};

Q_DECLARE_METATYPE(WindowGroup) // for QVariant

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
    explicit Widget(QWidget* parent = nullptr);
    QList<WindowGroup> prepareWindowGroupList();
    bool prepareListWidget();
    Q_INVOKABLE bool requestShow();
    void notifyForegroundChanged(HWND hwnd, ForegroundChangeSource source);

    HWND hWnd() { return (HWND) winId(); }

    bool isForeground() { return GetForegroundWindow() == hWnd(); }

    ~Widget() override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void rotateTaskbarWindowInGroup(const QString& exePath, bool forward, int windows);
    void clearGroupWindowOrder();

private:
    bool forceShow();
    void showLabelForItem(const QModelIndex& index, QString text = QString());
    void setupLabelFont();
    auto getLastActiveGroupWindow(const QString& exePath) -> QPair<HWND, QDateTime>;
    auto getLastValidActiveGroupWindow(const WindowGroup& group) -> QPair<HWND, QDateTime>;
    void sortGroupWindows(QList<HWND>& windows, const QString& exePath);
    QList<HWND> buildGroupWindowOrder(const QString& exePath);
    static HWND rotateWindowInGroup(const QList<HWND>& windows, HWND current, bool forward = true);
    static HWND rotateNormalWindowInGroup(const QList<HWND>& windows, HWND current, bool forward = true);

private:
    Ui::Widget* ui;
    QListView* lv = nullptr;
    WindowGroupModel* m_model = nullptr;
    const QMargins ListWidgetMargin{24, 24, 24, 24};
    /// exePath -> (HWND, time)
    QHash<QString, QHash<HWND, QDateTime>> winActiveOrder;
    QList<HWND> groupWindowOrder; // for Alt+` 同组窗口切换

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
