#include <QApplication>
#include <QAbstractNativeEventFilter>
#include <windows.h>
#include <QTimer>
#include <QMessageBox>
#include <QKeyEvent>
#include <qoperatingsystemversion.h>
#include <QStyleHints>
#include "UpdateDialog.h"
#include "widget.h"
#include "WindowManager.h"
#include "TaskbarWindowCycler.h"
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
#include "utils/ConfigManager.h"
#include "utils/QuitReason.h"
#include <shellapi.h>

struct SessionMonitor : QAbstractNativeEventFilter {
    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr*) override {
        if (eventType == "windows_generic_MSG") {
            MSG* msg = static_cast<MSG*>(message);
            if (msg->message == WM_QUERYENDSESSION) {
                qInfo() << "[Session] WM_QUERYENDSESSION - Windows session ending";
            } else if (msg->message == WM_ENDSESSION) {
                qInfo() << "[Session] WM_ENDSESSION - session ended:" << (bool)msg->wParam;
            }
        }
        return false;
    }
};

int main(int argc, char* argv[]) {
    QApplication a(argc, argv);
    Util::Logger::init();
    SingleApp singleApp("AltTaber-MrBeanCpp");
    if (singleApp.isRunning()) {
        qWarning() << "Another instance is running! Exit";
        QMessageBox::warning(nullptr, "Warning", "AltTaber is already running!");
        return 0;
    }

    ComInitializer com;

    qDebug() << "isUserAdmin" << IsUserAnAdmin();
    qDebug() << "System Version" << QOperatingSystemVersion::current().version();
    sysTray().show();
    UpdateDialog::verifyUpdate(a);
    initLanguage();

    if (cfg().getAlwaysRunAsAdmin() && !IsUserAnAdmin()) {
        QString appPath = QApplication::applicationFilePath();
        ShellExecuteW(nullptr, L"runas", (LPCWSTR)appPath.utf16(), nullptr, nullptr, SW_SHOWNORMAL);
        return 0;
    }

    QApplication::styleHints()->setColorScheme(
        ThemeManager::resolveTheme() == Light ? Qt::ColorScheme::Light : Qt::ColorScheme::Dark);
    qApp->setQuitOnLastWindowClosed(false);
    auto* wm = new WindowManager;
    auto* winSwitcher = new Widget(wm);
    wm->setSelfHwnd((HWND) winSwitcher->winId());

    QObject::connect(&sysTray(), &SystemTray::showRequested, winSwitcher, [winSwitcher]() {
        qInfo() << "[Main] System tray showRequested";
        winSwitcher->requestShow(OverlayIntent::ShowSwitcher);
    });

    SessionMonitor sessionMon;
    a.installNativeEventFilter(&sessionMon);

    QObject::connect(&a, &QApplication::aboutToQuit, []() {
        if (QuitReason::isIntentional()) {
            qInfo() << "[Quit] Intentional quit via menu or update";
        } else {
            qInfo() << "[Quit] Unexpected quit (session end, external termination, or crash)";
        }
        unhookWinEvent();
        Util::Logger::shutdown();
    });

    // Load hotkey bindings (with defaults merged in)
    auto bindings = cfg().effectiveHotkeyBindings();
    qInfo() << "[Main] Loaded" << bindings.size() << "hotkey actions";

    KeyboardHooker kbHooker((HWND) winSwitcher->winId());
    kbHooker.updateBindings(bindings);
    kbHooker.setPaused(cfg().getPaused());
    winSwitcher->updateOverlayBindings(bindings);

    TaskbarWheelHooker tbHooker;
    tbHooker.setPaused(cfg().getPaused());

    qInfo() << "[Main] Connecting kbHooker::hotkeyTriggered -> Widget::handleGlobalAction (QueuedConnection)";
    auto conn1 = QObject::connect(&kbHooker, &KeyboardHooker::hotkeyTriggered,
                                  winSwitcher, &Widget::handleGlobalAction, Qt::QueuedConnection);
    qInfo() << "[Main]   hotkeyTriggered connected:" << (bool)conn1;

    qInfo() << "[Main] Connecting kbHooker::activationModifiersReleased -> Widget::onActivationModifiersReleased (QueuedConnection)";
    auto conn2 = QObject::connect(&kbHooker, &KeyboardHooker::activationModifiersReleased,
                                  winSwitcher, &Widget::onActivationModifiersReleased,
                                  Qt::QueuedConnection);
    qInfo() << "[Main]   activationModifiersReleased connected:" << (bool)conn2;

    qInfo() << "[Main] Connecting Widget::overlayDismissed -> kbHooker::resetActivationModifiers";
    auto connReset = QObject::connect(winSwitcher, &Widget::overlayDismissed,
                                      &kbHooker, &KeyboardHooker::resetActivationModifiers);
    qInfo() << "[Main]   overlayDismissed connected:" << (bool)connReset;

    // Re-inject bindings when config is saved (settings dialog or notepad edit)
    QObject::connect(&cfg(), &ConfigManager::configEdited, &a, [&kbHooker, &tbHooker, winSwitcher]() {
        qInfo() << "[Config] configEdited -> re-injecting hotkey bindings";
        auto b = cfg().effectiveHotkeyBindings();
        qInfo() << "[Config] binding count:" << b.size() << "paused:" << cfg().getPaused();
        kbHooker.updateBindings(b);
        kbHooker.setPaused(cfg().getPaused());
        tbHooker.setPaused(cfg().getPaused());
        winSwitcher->updateOverlayBindings(b);
        kbHooker.resetActivationModifiers();
    });

    qInfo() << "[Main] Connecting tbHooker::tabWheelEvent -> TaskbarWindowCycler::rotate";
    auto conn3 = QObject::connect(&tbHooker, &TaskbarWheelHooker::tabWheelEvent,
                                   winSwitcher->taskbarCycler(), &TaskbarWindowCycler::rotate, Qt::QueuedConnection);
    qInfo() << "[Main]   tabWheelEvent connected:" << (bool)conn3;

    qInfo() << "[Main] Connecting tbHooker::leaveTaskbar -> TaskbarWindowCycler::clearOrder";
    auto conn4 = QObject::connect(&tbHooker, &TaskbarWheelHooker::leaveTaskbar,
                                   winSwitcher->taskbarCycler(), &TaskbarWindowCycler::clearOrder, Qt::QueuedConnection);
    qInfo() << "[Main]   leaveTaskbar connected:" << (bool)conn4;

    qInfo() << "[Main] Installing WinEvent hook for EVENT_SYSTEM_FOREGROUND";
    setWinEventHook([winSwitcher](DWORD event, HWND hwnd) {
        if (event == EVENT_SYSTEM_FOREGROUND) {
            winSwitcher->notifyForegroundChanged(hwnd);

            if (IsWindowVisible((HWND)winSwitcher->winId()) && hwnd != (HWND)winSwitcher->winId()
                && !Util::isKeyPressed(VK_MENU)
                && winSwitcher->overlayState() != Widget::OverlayState::Visible) {
                qInfo() << "[WinEvent] hiding overlay (fg changed while not Visible)";
                winSwitcher->hideOverlay();
            }

            auto className = Util::getClassName(hwnd);
            if (hwnd == GetForegroundWindow() && Util::isKeyPressed(VK_MENU) &&
                (className == "ForegroundStaging") &&
                winSwitcher->overlayState() != Widget::OverlayState::Visible) {
                qInfo() << "[WinEvent] Task switcher detected (fallback)" << className;
                winSwitcher->requestShow(OverlayIntent::FallbackShow);
            }
        }
    });

    qInfo() << "[Main] Scheduling warmupCache via singleShot(0)";
    QTimer::singleShot(0, winSwitcher, &Widget::warmupCache);

    qInfo() << "[Main] Entering event loop";
    return QApplication::exec();
}
