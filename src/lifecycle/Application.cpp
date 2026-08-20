#include <windows.h>
#include <shellapi.h>
#include <ShlObj_core.h>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QStyleHints>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QLocalSocket>
#include <QSharedMemory>
#include <QThread>

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
#include "lifecycle/IpcServer.h"
#include "core/LanguageManager.h"
#include "lifecycle/Logger.h"
#include "core/ThemeManager.h"
#include "core/StyleManager.h"
#include "core/ConfigManager.h"
#include "utils/VcRuntimeCheck.h"
#include "core/HotkeyAction.h"
#include "core/QuitReason.h"
#include "UpdateDialog.h"

bool SessionMonitor::nativeEventFilter(const QByteArray& eventType, void* message, qintptr*) {
    if (eventType == "windows_generic_MSG") {
        MSG* msg = static_cast<MSG*>(message);
        switch (msg->message) {
        case WM_QUERYENDSESSION:
            qInfo() << "[Session] WM_QUERYENDSESSION - Windows session ending";
            break;
        case WM_ENDSESSION:
            qInfo() << "[Session] WM_ENDSESSION - session ended:" << (bool)msg->wParam;
            break;
        case WM_SETTINGCHANGE:
        case WM_THEMECHANGED:
            ThemeManager::checkSystemThemeChange();
            break;
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

    if (!VcRuntimeCheck::isRuntimeAvailable()) {
        qWarning() << "[Main] VC++ runtime is not available";
        const QString url = QString::fromWCharArray(VcRuntimeCheck::vcRedistDownloadUrl());

        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setWindowTitle(QCoreApplication::translate("Application", "AltTaber"));
        msgBox.setText(QCoreApplication::translate("Application",
            "Microsoft Visual C++ Runtime is required but was not found. "
            "Please download and install it, then restart AltTaber."));
        msgBox.setInformativeText(url);
        msgBox.addButton(
            QCoreApplication::translate("Application", "Open download page"),
            QMessageBox::AcceptRole);
        msgBox.addButton(
            QCoreApplication::translate("Application", "Exit"),
            QMessageBox::RejectRole);
        msgBox.exec();

        if (msgBox.buttonRole(msgBox.clickedButton()) == QMessageBox::AcceptRole)
            QDesktopServices::openUrl(QUrl(url));

        QMetaObject::invokeMethod(&m_app, &QApplication::quit, Qt::QueuedConnection);
        return;
    }

    m_updateService = new UpdateService;
    if (m_updateService->handleUpdateRollback()) {
        qWarning() << "[Main] Update rollback handled, exiting";
        return;
    }

    initLanguage();

    m_singleApp = new SingleApp("AltTaber-MrBeanCpp");
    if (m_singleApp->isRunning()) {
        qWarning() << "Another instance is running!";

        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setWindowTitle(QCoreApplication::translate("Application", "AltTaber"));
        msgBox.setText(QCoreApplication::translate("Application", "AltTaber is already running!"));
        msgBox.addButton(
            QCoreApplication::translate("Application", "Close old instance and run"),
            QMessageBox::AcceptRole);
        msgBox.addButton(
            QCoreApplication::translate("Application", "Exit"),
            QMessageBox::RejectRole);
        msgBox.exec();

        if (msgBox.buttonRole(msgBox.clickedButton()) == QMessageBox::AcceptRole) {
            qInfo() << "Sending quit to running instance via IPC";
            QLocalSocket socket;
            socket.connectToServer("AltTaber-MrBeanCpp-IPC");
            if (socket.waitForConnected(2000)) {
                socket.write("quit\n");
                socket.waitForBytesWritten(2000);
                socket.waitForReadyRead(2000);
                qInfo() << "IPC response:" << socket.readAll().trimmed();
                socket.disconnectFromServer();
            }

            // Wait for old instance to release shared memory
            qInfo() << "Waiting for old instance to release shared memory...";
            QElapsedTimer timer;
            timer.start();
            while (timer.elapsed() < 5000) {
                QSharedMemory shm("AltTaber-MrBeanCpp");
                if (!shm.attach())
                    break;
                shm.detach();
                QThread::msleep(200);
            }

            // Retry: recreate SingleApp and check again
            delete m_singleApp;
            m_singleApp = new SingleApp("AltTaber-MrBeanCpp");
            if (m_singleApp->isRunning()) {
                qCritical() << "Failed to close old instance";
                QMessageBox::critical(nullptr,
                    QCoreApplication::translate("Application", "Error"),
                    QCoreApplication::translate("Application", "Failed to close old instance."));
                QMetaObject::invokeMethod(&m_app, &QApplication::quit, Qt::QueuedConnection);
                return;
            }
            qInfo() << "Old instance closed, proceeding with startup";
        } else {
            QMetaObject::invokeMethod(&m_app, &QApplication::quit, Qt::QueuedConnection);
            return;
        }
    }

    m_com = new ComInitializer;

    sysTray().show();
    UpdateDialog::verifyUpdate(m_app);

    if (m_config->getAlwaysRunAsAdmin() && !Util::isUserAdmin()) {
        qWarning() << "[Main] Not running as admin, relaunching with runas";
        QString appPath = QApplication::applicationFilePath();
        ShellExecuteW(nullptr, L"runas", (LPCWSTR)appPath.utf16(), nullptr, nullptr, SW_SHOWNORMAL);
        QMetaObject::invokeMethod(&m_app, &QApplication::quit, Qt::QueuedConnection);
        return;
    }

    m_ipcServer = new IpcServer;
    m_ipcServer->start();
    QObject::connect(m_ipcServer, &IpcServer::quitRequested, &m_app, [this]() {
        QuitReason::markIntentional();
        QTimer::singleShot(50, &m_app, &QApplication::quit);
    });

    auto bindings = m_config->effectiveHotkeyBindings();
    qInfo() << "[Main] Loaded" << bindings.size() << "hotkey actions";

    QElapsedTimer phaseTimer, totalTimer;
    phaseTimer.start();
    totalTimer.start();
    initControllers();
    {
        auto e = phaseTimer.restart();
        qInfo() << "[Startup] initControllers" << e << "ms";
        Util::checkSlowInit("initControllers", e, 100);
    }

    initUI();
    {
        auto e = phaseTimer.restart();
        qInfo() << "[Startup] initUI" << e << "ms";
        Util::checkSlowInit("initUI", e, 100);
    }

    initHotkeys();
    {
        auto e = phaseTimer.restart();
        qInfo() << "[Startup] initHotkeys" << e << "ms";
        Util::checkSlowInit("initHotkeys", e, 100);
    }

    QObject::connect(m_config, &ConfigManager::configEdited, &m_app, [this]() {
        m_hotkeyService->reloadFromConfig();
    });

    {
        QElapsedTimer t;
        t.start();
        StyleManager::applyTheme(ThemeManager::current());
        if (m_widget) {
            m_widget->style()->unpolish(m_widget);
            m_widget->style()->polish(m_widget);
            m_widget->update();
        }
        auto e = t.elapsed();
        qInfo() << "[Startup] applyTheme+polish" << e << "ms";
        Util::checkSlowInit("applyTheme+polish", e, 100);
    }

    QObject::connect(&ThemeManager::instance(), &ThemeManager::themeChanged, qApp, []() {
        StyleManager::applyTheme(ThemeManager::current());
        sysTray().refreshStyle();
    });

    if (m_widget) {
        QTimer::singleShot(0, m_widget, &Widget::warmupCache);
    }

    m_updateService->cleanupUpdateMarkers();
    {
        auto e = totalTimer.elapsed();
        qInfo() << "[Startup] Application constructor total" << e << "ms";
        Util::checkSlowInit("Application constructor total", e, 500);
    }
    qInfo() << "[Main] Entering event loop";
}

Application::~Application() {
    unhookWinEvent();
    Util::Logger::shutdown();
    delete m_ipcServer;
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
        // Tray show is an explicit, stay-open session: the overlay stays until
        // the user picks a window (click/Enter) or dismisses it (Esc). Using
        // ShowSwitcherStayOpen keeps clicks/Enter usable and out of the
        // modifier-release watchdog's scope.
        m_widget->requestShow(OverlayIntent::ShowSwitcher, HotkeyAction::ShowSwitcherStayOpen);
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
