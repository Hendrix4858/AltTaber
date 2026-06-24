#include <windows.h>
#include <shellapi.h>
#include <ShlObj_core.h>
#include <QMessageBox>
#include <qoperatingsystemversion.h>
#include <QStyleHints>
#include <QProcess>
#include <QDir>
#include <QCoreApplication>
#include <QElapsedTimer>

#include "lifecycle/Application.h"
#include "ActionRouter.h"
#include "widget.h"
#include "SelectionController.h"
#include "GroupWindowCycler.h"
#include "WindowManager.h"
#include "hook/winEventHook.h"
#include "utils/Util.h"
#include "lifecycle/ComInitializer.h"
#include "lifecycle/SingleApp.h"
#include "lifecycle/SystemTray.h"
#include "lifecycle/UpdateService.h"
#include "lifecycle/HotkeyService.h"
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
    m_app.setApplicationVersion(APP_VERSION);
    m_config = &cfg();
    Util::Logger::init();

    m_updateService = new UpdateService;
    if (m_updateService->handleUpdateRollback())
        return;

    m_singleApp = new SingleApp("AltTaber-MrBeanCpp");
    if (m_singleApp->isRunning()) {
        qWarning() << "Another instance is running! Exit";
        QMessageBox::warning(nullptr, "Warning", "AltTaber is already running!");
        QMetaObject::invokeMethod(&m_app, &QApplication::quit, Qt::QueuedConnection);
        return;
    }

    m_com = new ComInitializer;

    qDebug() << "isUserAdmin" << Util::isUserAdmin();
    qDebug() << "System Version" << QOperatingSystemVersion::current().version();
    sysTray().show();
    UpdateDialog::verifyUpdate(m_app);
    initLanguage();

    if (m_config->getAlwaysRunAsAdmin() && !Util::isUserAdmin()) {
        QString appPath = QApplication::applicationFilePath();
        ShellExecuteW(nullptr, L"runas", (LPCWSTR)appPath.utf16(), nullptr, nullptr, SW_SHOWNORMAL);
        QMetaObject::invokeMethod(&m_app, &QApplication::quit, Qt::QueuedConnection);
        return;
    }

    auto bindings = m_config->effectiveHotkeyBindings();
    qInfo() << "[Main] Loaded" << bindings.size() << "hotkey actions";

    QElapsedTimer phaseTimer, totalTimer;
    phaseTimer.start();
    totalTimer.start();
    initControllers();
    qInfo() << "[Startup] initControllers" << phaseTimer.restart() << "ms";

    initUI();
    qInfo() << "[Startup] initUI" << phaseTimer.restart() << "ms";

    initHotkeys();
    qInfo() << "[Startup] initHotkeys" << phaseTimer.restart() << "ms";

    QObject::connect(m_config, &ConfigManager::configEdited, &m_app, [this]() {
        m_hotkeyService->reloadFromConfig();
    });

    if (m_widget) {
        qInfo() << "[Main] Scheduling warmupCache via singleShot(0)";
        QTimer::singleShot(0, m_widget, &Widget::warmupCache);
    }

    m_updateService->cleanupUpdateMarkers();
    qInfo() << "[Startup] Application constructor total" << totalTimer.elapsed() << "ms";
    qInfo() << "[Main] Entering event loop";
}

Application::~Application() {
    unhookWinEvent();
    Util::Logger::shutdown();
    delete m_singleApp;
    delete m_com;
    delete m_updateService;
}

void Application::initControllers() {
    QApplication::styleHints()->setColorScheme(
        ThemeManager::resolveTheme() == Light ? Qt::ColorScheme::Light : Qt::ColorScheme::Dark);
    qApp->setQuitOnLastWindowClosed(false);

    m_windowManager = new WindowManager(m_config);
    m_widget = new Widget(m_windowManager);
    m_windowManager->setSelfHwnd(m_widget->hWnd());
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

void Application::initHotkeys() {
    auto bindings = m_config->effectiveHotkeyBindings();

    auto* router = new ActionRouter(&m_app);
    router->setOverlayController(m_widget->overlayController());
    router->setSelectionController(m_widget->selectionController());
    router->setGroupWindowCycler(m_widget->groupWindowCycler());
    router->setWindowManager(m_windowManager);

    m_hotkeyService = new HotkeyService(m_config, &m_app);
    m_hotkeyService->init(m_widget, router, bindings);
    m_hotkeyService->wireSignals(m_widget);
}

int Application::run() {
    return QApplication::exec();
}
