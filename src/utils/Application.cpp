#include <windows.h>
#include <shellapi.h>
#include <ShlObj_core.h>
#include <QMessageBox>
#include <qoperatingsystemversion.h>
#include <QStyleHints>

#include "utils/Application.h"
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
#include "UpdateDialog.h"

bool SessionMonitor::nativeEventFilter(const QByteArray& eventType, void* message, qintptr*) {
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

Application::Application(int argc, char* argv[])
    : m_app(argc, argv) {
    Util::Logger::init();

    m_singleApp = new SingleApp("AltTaber-MrBeanCpp");
    if (m_singleApp->isRunning()) {
        qWarning() << "Another instance is running! Exit";
        QMessageBox::warning(nullptr, "Warning", "AltTaber is already running!");
        QMetaObject::invokeMethod(&m_app, &QApplication::quit, Qt::QueuedConnection);
        return;
    }

    m_com = new ComInitializer;

    qDebug() << "isUserAdmin" << IsUserAnAdmin();
    qDebug() << "System Version" << QOperatingSystemVersion::current().version();
    sysTray().show();
    UpdateDialog::verifyUpdate(m_app);
    initLanguage();

    if (cfg().getAlwaysRunAsAdmin() && !IsUserAnAdmin()) {
        QString appPath = QApplication::applicationFilePath();
        ShellExecuteW(nullptr, L"runas", (LPCWSTR)appPath.utf16(), nullptr, nullptr, SW_SHOWNORMAL);
        QMetaObject::invokeMethod(&m_app, &QApplication::quit, Qt::QueuedConnection);
        return;
    }

    auto bindings = cfg().effectiveHotkeyBindings();
    qInfo() << "[Main] Loaded" << bindings.size() << "hotkey actions";

    initConfig(bindings);
    initControllers();
    initUI();
    initHooks(bindings);
    wireSignals(bindings);

    if (m_winSwitcher) {
        qInfo() << "[Main] Scheduling warmupCache via singleShot(0)";
        QTimer::singleShot(0, m_winSwitcher, &Widget::warmupCache);
    }

    qInfo() << "[Main] Entering event loop";
}

Application::~Application() {
    unhookWinEvent();
    Util::Logger::shutdown();
    delete m_singleApp;
    delete m_com;
}

void Application::initConfig(const HotkeyBindings& bindings) {
    Q_UNUSED(bindings);
}

void Application::initControllers() {
    QApplication::styleHints()->setColorScheme(
        ThemeManager::resolveTheme() == Light ? Qt::ColorScheme::Light : Qt::ColorScheme::Dark);
    qApp->setQuitOnLastWindowClosed(false);

    m_wm = new WindowManager;
    m_winSwitcher = new Widget(m_wm);
    m_wm->setSelfHwnd((HWND) m_winSwitcher->winId());
}

void Application::initUI() {
    QObject::connect(&sysTray(), &SystemTray::showRequested, m_winSwitcher, [this]() {
        qInfo() << "[Main] System tray showRequested";
        m_winSwitcher->requestShow(OverlayIntent::ShowSwitcher);
    });

    m_app.installNativeEventFilter(&m_sessionMon);

    QObject::connect(&m_app, &QApplication::aboutToQuit, []() {
        if (QuitReason::isIntentional()) {
            qInfo() << "[Quit] Intentional quit via menu or update";
        } else {
            qInfo() << "[Quit] Unexpected quit (session end, external termination, or crash)";
        }
        unhookWinEvent();
        Util::Logger::shutdown();
    });
}

void Application::initHooks(const HotkeyBindings& bindings) {
    m_kbHooker = new KeyboardHooker((HWND) m_winSwitcher->winId());
    m_kbHooker->updateBindings(bindings);
    m_kbHooker->setPaused(cfg().getPaused());
    m_winSwitcher->updateOverlayBindings(bindings);

    m_tbHooker = new TaskbarWheelHooker;
    m_tbHooker->setPaused(cfg().getPaused());

    qInfo() << "[Main] Installing WinEvent hook for EVENT_SYSTEM_FOREGROUND";
    setWinEventHook([this](DWORD event, HWND hwnd) {
        if (event == EVENT_SYSTEM_FOREGROUND) {
            m_winSwitcher->notifyForegroundChanged(hwnd);

            auto oState = m_winSwitcher->overlayController()->overlayState();
            if (oState != OverlayController::OverlayState::Hidden
                && hwnd != (HWND)m_winSwitcher->winId()
                && !Util::isKeyPressed(VK_MENU)
                && oState != OverlayController::OverlayState::Visible) {
                qInfo() << "[WinEvent] hiding overlay (fg changed while not Visible)";
                m_winSwitcher->hideOverlay();
            }

            auto className = Util::getClassName(hwnd);
            if (hwnd == GetForegroundWindow() && Util::isKeyPressed(VK_MENU) &&
                (className == "ForegroundStaging") &&
                m_winSwitcher->overlayController()->overlayState()
                    != OverlayController::OverlayState::Visible) {
                qInfo() << "[WinEvent] Task switcher detected (fallback)" << className;
                m_winSwitcher->requestShow(OverlayIntent::FallbackShow);
            }
        }
    });
}

void Application::wireSignals(const HotkeyBindings& bindings) {
    Q_UNUSED(bindings);

    qInfo() << "[Main] Connecting kbHooker::hotkeyTriggered -> Widget::handleGlobalAction (QueuedConnection)";
    auto conn1 = QObject::connect(m_kbHooker, &KeyboardHooker::hotkeyTriggered,
                                  m_winSwitcher, &Widget::handleGlobalAction, Qt::QueuedConnection);
    qInfo() << "[Main]   hotkeyTriggered connected:" << (bool)conn1;

    qInfo() << "[Main] Connecting kbHooker::overlayKeyTriggered -> Widget::handleHookOverlayAction (QueuedConnection)";
    auto connHook = QObject::connect(m_kbHooker, &KeyboardHooker::overlayKeyTriggered,
                                     m_winSwitcher, &Widget::handleHookOverlayAction,
                                     Qt::QueuedConnection);
    qInfo() << "[Main]   overlayKeyTriggered connected:" << (bool)connHook;

    qInfo() << "[Main] Connecting kbHooker::activationModifiersReleased -> Widget::onActivationModifiersReleased (QueuedConnection)";
    auto conn2 = QObject::connect(m_kbHooker, &KeyboardHooker::activationModifiersReleased,
                                  m_winSwitcher, &Widget::onActivationModifiersReleased,
                                  Qt::QueuedConnection);
    qInfo() << "[Main]   activationModifiersReleased connected:" << (bool)conn2;

    qInfo() << "[Main] Connecting Widget::overlayDismissed -> kbHooker::resetActivationModifiers";
    auto connReset = QObject::connect(m_winSwitcher, &Widget::overlayDismissed,
                                      m_kbHooker, &KeyboardHooker::resetActivationModifiers);
    qInfo() << "[Main]   overlayDismissed connected:" << (bool)connReset;

    qInfo() << "[Main] Connecting Widget::overlayShown -> kbHooker::notifyOverlayShown";
    auto connShown = QObject::connect(m_winSwitcher, &Widget::overlayShown,
                                      m_kbHooker, &KeyboardHooker::notifyOverlayShown);
    qInfo() << "[Main]   overlayShown connected:" << (bool)connShown;

    // Re-inject bindings when config is saved
    QObject::connect(&cfg(), &ConfigManager::configEdited, &m_app, [this]() {
        qInfo() << "[Config] configEdited -> re-injecting hotkey bindings";
        auto b = cfg().effectiveHotkeyBindings();
        qInfo() << "[Config] binding count:" << b.size() << "paused:" << cfg().getPaused();
        m_kbHooker->updateBindings(b);
        m_kbHooker->setPaused(cfg().getPaused());
        m_tbHooker->setPaused(cfg().getPaused());
        m_winSwitcher->updateOverlayBindings(b);
        m_kbHooker->resetActivationModifiers();
    });

    auto conn3 = QObject::connect(m_tbHooker, &TaskbarWheelHooker::tabWheelEvent,
                                   m_winSwitcher->taskbarCycler(), &TaskbarWindowCycler::rotate, Qt::QueuedConnection);
    qInfo() << "[Main]   tabWheelEvent connected:" << (bool)conn3;

    qInfo() << "[Main] Connecting tbHooker::leaveTaskbar -> TaskbarWindowCycler::clearOrder";
    auto conn4 = QObject::connect(m_tbHooker, &TaskbarWheelHooker::leaveTaskbar,
                                   m_winSwitcher->taskbarCycler(), &TaskbarWindowCycler::clearOrder, Qt::QueuedConnection);
    qInfo() << "[Main]   leaveTaskbar connected:" << (bool)conn4;
}

int Application::run() {
    return QApplication::exec();
}
