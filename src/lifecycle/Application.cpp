#include <windows.h>
#include <shellapi.h>
#include <ShlObj_core.h>
#include <QMessageBox>
#include <qoperatingsystemversion.h>
#include <QStyleHints>

#include "lifecycle/Application.h"
#include "widget.h"
#include "WindowManager.h"
#include "TaskbarWindowCycler.h"
#include "hook/winEventHook.h"
#include "utils/Util.h"
#include "hook/TaskbarWheelHooker.h"
#include "hook/KeyboardHooker.h"
#include "lifecycle/ComInitializer.h"
#include "lifecycle/SingleApp.h"
#include "lifecycle/SystemTray.h"
#include "core/LanguageManager.h"
#include "lifecycle/Logger.h"
#include "core/ThemeManager.h"
#include "core/ConfigManager.h"
#include "core/QuitReason.h"
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
    m_app.setOrganizationName("MrBeanCpp");
    m_app.setApplicationName("AltTaber");
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

    if (m_widget) {
        qInfo() << "[Main] Scheduling warmupCache via singleShot(0)";
        QTimer::singleShot(0, m_widget, &Widget::warmupCache);
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

    m_windowManager = new WindowManager;
    m_widget = new Widget(m_windowManager);
    m_windowManager->setSelfHwnd((HWND) m_widget->winId());
}

void Application::initUI() {
    QObject::connect(&sysTray(), &SystemTray::showRequested, m_widget, [this]() {
        qInfo() << "[Main] System tray showRequested";
        m_widget->requestShow(OverlayIntent::ShowSwitcher);
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
    m_keyboardHooker = new KeyboardHooker((HWND) m_widget->winId());
    m_keyboardHooker->updateBindings(bindings);
    m_keyboardHooker->setPaused(cfg().getPaused());
    m_widget->updateOverlayBindings(bindings);

    m_taskbarHooker = new TaskbarWheelHooker;
    m_taskbarHooker->setPaused(cfg().getPaused());

    qInfo() << "[Main] Installing WinEvent hook for EVENT_SYSTEM_FOREGROUND";
    setWinEventHook([this](DWORD event, HWND hwnd) {
        if (event == EVENT_SYSTEM_FOREGROUND) {
            m_widget->notifyForegroundChanged(hwnd);

            auto oState = m_widget->overlayController()->overlayState();
            if (oState != OverlayController::OverlayState::Hidden
                && hwnd != (HWND)m_widget->winId()
                && !Util::isKeyPressed(VK_MENU)
                && oState != OverlayController::OverlayState::Visible) {
                qInfo() << "[WinEvent] hiding overlay (fg changed while not Visible)";
                m_widget->hideOverlay();
            }

            auto className = Util::getClassName(hwnd);
            if (hwnd == GetForegroundWindow() && Util::isKeyPressed(VK_MENU) &&
                (className == "ForegroundStaging") &&
                m_widget->overlayController()->overlayState()
                    != OverlayController::OverlayState::Visible) {
                qInfo() << "[WinEvent] Task switcher detected (fallback)" << className;
                m_widget->requestShow(OverlayIntent::FallbackShow);
            }
        }
    });
}

void Application::wireSignals(const HotkeyBindings& bindings) {
    Q_UNUSED(bindings);

    qInfo() << "[Main] Connecting kbHooker::hotkeyTriggered -> Widget::handleGlobalAction (QueuedConnection)";
    auto conn1 = QObject::connect(m_keyboardHooker, &KeyboardHooker::hotkeyTriggered,
                                  m_widget, &Widget::handleGlobalAction, Qt::QueuedConnection);
    qInfo() << "[Main]   hotkeyTriggered connected:" << (bool)conn1;

    qInfo() << "[Main] Connecting kbHooker::overlayKeyTriggered -> Widget::handleHookOverlayAction (QueuedConnection)";
    auto connHook = QObject::connect(m_keyboardHooker, &KeyboardHooker::overlayKeyTriggered,
                                     m_widget, &Widget::handleHookOverlayAction,
                                     Qt::QueuedConnection);
    qInfo() << "[Main]   overlayKeyTriggered connected:" << (bool)connHook;

    qInfo() << "[Main] Connecting kbHooker::activationModifiersReleased -> Widget::onActivationModifiersReleased (QueuedConnection)";
    auto conn2 = QObject::connect(m_keyboardHooker, &KeyboardHooker::activationModifiersReleased,
                                  m_widget, &Widget::onActivationModifiersReleased,
                                  Qt::QueuedConnection);
    qInfo() << "[Main]   activationModifiersReleased connected:" << (bool)conn2;

    qInfo() << "[Main] Connecting Widget::overlayDismissed -> kbHooker::resetActivationModifiers";
    auto connReset = QObject::connect(m_widget, &Widget::overlayDismissed,
                                      m_keyboardHooker, &KeyboardHooker::resetActivationModifiers);
    qInfo() << "[Main]   overlayDismissed connected:" << (bool)connReset;

    qInfo() << "[Main] Connecting Widget::overlayShown -> kbHooker::notifyOverlayShown";
    auto connShown = QObject::connect(m_widget, &Widget::overlayShown,
                                      m_keyboardHooker, &KeyboardHooker::notifyOverlayShown);
    qInfo() << "[Main]   overlayShown connected:" << (bool)connShown;

    // Re-inject bindings when config is saved
    QObject::connect(&cfg(), &ConfigManager::configEdited, &m_app, [this]() {
        qInfo() << "[Config] configEdited -> re-injecting hotkey bindings";
        auto b = cfg().effectiveHotkeyBindings();
        qInfo() << "[Config] binding count:" << b.size() << "paused:" << cfg().getPaused();
        m_keyboardHooker->updateBindings(b);
        m_keyboardHooker->setPaused(cfg().getPaused());
        m_taskbarHooker->setPaused(cfg().getPaused());
        m_widget->updateOverlayBindings(b);
        m_keyboardHooker->resetActivationModifiers();
    });

    auto conn3 = QObject::connect(m_taskbarHooker, &TaskbarWheelHooker::tabWheelEvent,
                                   m_widget->taskbarCycler(), &TaskbarWindowCycler::rotate, Qt::QueuedConnection);
    qInfo() << "[Main]   tabWheelEvent connected:" << (bool)conn3;

    qInfo() << "[Main] Connecting tbHooker::leaveTaskbar -> TaskbarWindowCycler::clearOrder";
    auto conn4 = QObject::connect(m_taskbarHooker, &TaskbarWheelHooker::leaveTaskbar,
                                   m_widget->taskbarCycler(), &TaskbarWindowCycler::clearOrder, Qt::QueuedConnection);
    qInfo() << "[Main]   leaveTaskbar connected:" << (bool)conn4;
}

int Application::run() {
    return QApplication::exec();
}
