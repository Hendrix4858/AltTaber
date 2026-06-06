#include <QApplication>
#include <windows.h>
#include <QTimer>
#include <QMessageBox>
#include <QKeyEvent>
#include <qoperatingsystemversion.h>
#include <QStyleHints>
#include "UpdateDialog.h"
#include "widget.h"
#include "WindowManager.h"
#include "utils/winEventHook.h"
#include "utils/Util.h"
#include "utils/TaskbarWheelHooker.h"
#include "utils/KeyboardHooker.h"
#include "utils/ComInitializer.h"
#include "utils/SingleApp.h"
#include "utils/SystemTray.h"
#include "utils/LanguageManager.h"
#include "utils/Logger.h"
#include "utils/ThemeManager.h"
#include <shellapi.h>

int main(int argc, char* argv[]) {
    QApplication a(argc, argv);
    Util::Logger::init();
    SingleApp singleApp("AltTaber-MrBeanCpp");
    if (singleApp.isRunning()) {
        qWarning() << "Another instance is running! Exit";
        QMessageBox::warning(nullptr, "Warning", "AltTaber is already running!");
        return 0;
    }

    // 其实Qt内部已经初始化了，这里是保险起见
    ComInitializer com; // 初始化COM组件 for 主线程
    qDebug() << qt_error_string(S_OK);

    qDebug() << "isUserAdmin" << IsUserAnAdmin();
    qDebug() << "System Version" << QOperatingSystemVersion::current().version();
    sysTray().show(); // show之后才能使用系统通知
    UpdateDialog::verifyUpdate(a); // 验证更新
    initLanguage();

    if (cfg().getAlwaysRunAsAdmin() && !IsUserAnAdmin()) {
        QString appPath = QApplication::applicationFilePath();
        ShellExecuteW(nullptr, L"runas", (LPCWSTR)appPath.utf16(), nullptr, nullptr, SW_SHOWNORMAL);
        return 0;
    }

    // 默认情况下，会根据系统主题自动切换; 但是一旦自定义qss，自动切换就会失效; 只好固定为Dark/Light
    QApplication::styleHints()->setColorScheme(
        ThemeManager::resolveTheme() == Light ? Qt::ColorScheme::Light : Qt::ColorScheme::Dark);
    qApp->setQuitOnLastWindowClosed(false);
    auto* wm = new WindowManager;
    auto* winSwitcher = new Widget(wm);
    wm->setSelfHwnd((HWND) winSwitcher->winId());
    winSwitcher->prepareListWidget(); // 优化：对ListWidget进行预先初始化，首次执行`setCurrentRow`特别耗时(472ms)

    QObject::connect(&a, &QApplication::aboutToQuit, []() {
        unhookWinEvent();
        Util::Logger::shutdown();
    });

    KeyboardHooker kbHooker((HWND) winSwitcher->winId());
    QObject::connect(&kbHooker, &KeyboardHooker::requestShow,
                     winSwitcher, &Widget::requestShow, Qt::QueuedConnection);
    QObject::connect(&kbHooker, &KeyboardHooker::altTabPressed, winSwitcher,
                     [winSwitcher](Qt::KeyboardModifiers mods) {
        // 已可见但不在前台（被 DebugView 等抢走前台）时，把 overlay 置前
        if (winSwitcher->isVisible() && !winSwitcher->isForeground()) {
            SetForegroundWindow((HWND)winSwitcher->winId());
        }
        auto event = new QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::AltModifier | mods);
        QApplication::postEvent(winSwitcher, event);
    }, Qt::QueuedConnection);
    QObject::connect(&kbHooker, &KeyboardHooker::altGravePressed, winSwitcher,
                     [winSwitcher](Qt::KeyboardModifiers mods) {
        auto event = new QKeyEvent(QEvent::KeyPress, Qt::Key_QuoteLeft, Qt::AltModifier | mods);
        QApplication::postEvent(winSwitcher, event);
    }, Qt::QueuedConnection);
    QObject::connect(&kbHooker, &KeyboardHooker::altArrowPressed, winSwitcher,
                     [winSwitcher](int vkCode) {
        if (winSwitcher->isVisible() && !winSwitcher->isForeground()) {
            SetForegroundWindow((HWND)winSwitcher->winId());
        }
        Qt::Key qtKey;
        switch (vkCode) {
            case VK_LEFT:  qtKey = Qt::Key_Left;  break;
            case VK_RIGHT: qtKey = Qt::Key_Right; break;
            case VK_UP:    qtKey = Qt::Key_Up;    break;
            case VK_DOWN:  qtKey = Qt::Key_Down;  break;
            default: return;
        }
        auto event = new QKeyEvent(QEvent::KeyPress, qtKey, Qt::AltModifier);
        QApplication::postEvent(winSwitcher, event);
    }, Qt::QueuedConnection);
    QObject::connect(&kbHooker, &KeyboardHooker::altReleased, winSwitcher,
                     [winSwitcher]() {
        auto event = new QKeyEvent(QEvent::KeyRelease, Qt::Key_Alt, Qt::NoModifier);
        QApplication::postEvent(winSwitcher, event);
    }, Qt::QueuedConnection);

    TaskbarWheelHooker tbHooker;
    QObject::connect(&tbHooker, &TaskbarWheelHooker::tabWheelEvent,
                     winSwitcher, &Widget::rotateTaskbarWindowInGroup, Qt::QueuedConnection);
    QObject::connect(&tbHooker, &TaskbarWheelHooker::leaveTaskbar,
                     winSwitcher, &Widget::clearGroupWindowOrder, Qt::QueuedConnection);

    setWinEventHook([winSwitcher](DWORD event, HWND hwnd) {
        // 某些情况下，Hook拦截不到Alt+Tab（如VMware获取焦点且虚拟机开启时，即便focus在标题栏上）
        // （GPT建议RegisterRawInputDevices，不知道有没有效果，感觉比较危险）
        // 此时需要通过监控前台窗口检测系统的任务切换窗口唤出，并弹出本程序
        // 一旦焦点脱离VMWare，Hook就能正常工作，接下里就可以正常拦截Alt+Tab
        // 而再次连续按下TAB（Alt按住的情况下），也不会出现反复弹出系统任务切换窗口的情况（如果不进行Hook拦截就会这样，所以两种方法结合使用）
        if (event == EVENT_SYSTEM_FOREGROUND/* && hwnd == GetForegroundWindow()*/) { // 前台窗口变化 TODO 记录窗口关闭事件
            // 使用[原生]Alt+TAB呼出任务切换窗口时，会触发两次EVENT_SYSTEM_FOREGROUND事件
            // 第二次是在目标窗口已经切换到前台后触发的，非常诡异
            // ^1 可以用 GetForegroundWindow() || isAltPressed 来二次确认 （或者过滤相邻相同hwnd）

            // 对于Follower启动的CMD，由于Follower先行隐藏，会导致焦点先回落到上个窗口，再到新窗口 （但是用Win键打开的cmd没事）
            // 但是此时GetForegroundWindow()还是上个窗口，可能是更新不及时，所以 ^1-1 处的方案不可行
            // 问题不大，本程序hook了Alt+Tab，已经不会出现两次Event了
            winSwitcher->notifyForegroundChanged(hwnd);

            // 覆盖层可见、新前台不是覆盖层、Alt未按下 → 隐藏覆盖层
            if (IsWindowVisible((HWND)winSwitcher->winId()) && hwnd != (HWND)winSwitcher->winId()
                && !Util::isKeyPressed(VK_MENU)) {
                QMetaObject::invokeMethod(winSwitcher, [winSwitcher]() {
                    winSwitcher->hide();
                }, Qt::QueuedConnection);
            }

            auto className = Util::getClassName(hwnd);
            // ForegroundStaging貌似是辅助过渡动画
            // 检测 Alt 按下，防止误判 Win+Tab (任务视图)
            if (hwnd == GetForegroundWindow() && Util::isKeyPressed(VK_MENU) &&
                (className == "ForegroundStaging" /*|| className == "XamlExplorerHostIslandWindow"*/)) { // 任务切换窗口
                // 顺序是ForegroundStaging -> XamlExplorerHostIslandWindow，不需要都检测，否则会重复
                // 且：XamlExplorerHostIslandWindow 会导致误检测（某些系统版本，任务栏app窗口>1时，点击窗口）
                qDebug() << "Task switcher detected" << className;
                // 事件驱动重试，替代原有的 Sleep 忙等
                auto tryShowSwitcher = [](Widget* w, auto&& self, int retries) -> void {
                    if (retries <= 0) return;
                    w->requestShow();
                    QTimer::singleShot(10, w, [w, self, retries]() {
                        if (!w->isForeground())
                            self(w, self, retries - 1);
                    });
                };
                tryShowSwitcher(winSwitcher, tryShowSwitcher, 5);
            }
        }
    });

    return QApplication::exec();
}
