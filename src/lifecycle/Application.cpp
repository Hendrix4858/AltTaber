#include <windows.h>
#include <shellapi.h>
#include <ShlObj_core.h>
#include <QMessageBox>
#include <qoperatingsystemversion.h>
#include <QStyleHints>
#include <QProcess>
#include <QDir>
#include <QCoreApplication>

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
#include "core/UpdateMarker.h"

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
    : m_app(argc, argv), m_config(&cfg()) {
    m_app.setOrganizationName("MrBeanCpp");
    m_app.setApplicationName("AltTaber");
    m_app.setApplicationVersion(APP_VERSION);
    Util::Logger::init();

    // --- Update rollback and cleanup ---
    {
        auto marker = UpdateMarker::read();

        if (marker == "ok" && UpdateMarker::hasBackup()) {
            qInfo() << "[Update] Cleaning up stale backup (previous update ok)";
            UpdateMarker::cleanupBackup();
        }

        if (marker == "pending" && UpdateMarker::hasBackup()) {
            int attempts = UpdateMarker::readRollbackCount();
            UpdateMarker::writeRollbackCount(attempts + 1);
            if (attempts >= 1) {
                qWarning() << "[Update] Prior init attempt detected, initiating rollback";
                if (tryRollback())
                    return;
            } else {
                qInfo() << "[Update] First attempt after update, proceeding normally";
            }
        }
    }

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

    if (m_config->getAlwaysRunAsAdmin() && !IsUserAnAdmin()) {
        QString appPath = QApplication::applicationFilePath();
        ShellExecuteW(nullptr, L"runas", (LPCWSTR)appPath.utf16(), nullptr, nullptr, SW_SHOWNORMAL);
        QMetaObject::invokeMethod(&m_app, &QApplication::quit, Qt::QueuedConnection);
        return;
    }

    auto bindings = m_config->effectiveHotkeyBindings();
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

    UpdateMarker::removeMarker();
    UpdateMarker::resetRollbackCount();
    if (UpdateMarker::hasBackup())
        UpdateMarker::cleanupBackup();
    qInfo() << "[Main] Update marker cleaned up, init complete";

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

    m_windowManager = new WindowManager(m_config);
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
    m_keyboardHooker->setPaused(m_config->getPaused());
    m_widget->updateOverlayBindings(bindings);

    m_taskbarHooker = new TaskbarWheelHooker;
    m_taskbarHooker->setPaused(m_config->getPaused());

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

    QObject::connect(m_keyboardHooker, &KeyboardHooker::hotkeyTriggered,
                     m_widget, &Widget::handleGlobalAction, Qt::QueuedConnection);

    QObject::connect(m_keyboardHooker, &KeyboardHooker::overlayKeyTriggered,
                     m_widget, &Widget::handleHookOverlayAction, Qt::QueuedConnection);

    QObject::connect(m_keyboardHooker, &KeyboardHooker::activationModifiersReleased,
                     m_widget, &Widget::onActivationModifiersReleased, Qt::QueuedConnection);

    QObject::connect(m_widget, &Widget::overlayDismissed,
                     m_keyboardHooker, &KeyboardHooker::resetActivationModifiers);

    QObject::connect(m_widget, &Widget::overlayShown,
                     m_keyboardHooker, &KeyboardHooker::notifyOverlayShown);

    // Re-inject bindings when config is saved
    QObject::connect(m_config, &ConfigManager::configEdited, &m_app, [this]() {
        qInfo() << "[Config] configEdited -> re-injecting hotkey bindings";
        auto b = m_config->effectiveHotkeyBindings();
        qInfo() << "[Config] binding count:" << b.size() << "paused:" << m_config->getPaused();
        m_keyboardHooker->updateBindings(b);
        m_keyboardHooker->setPaused(m_config->getPaused());
        m_taskbarHooker->setPaused(m_config->getPaused());
        m_widget->updateOverlayBindings(b);
        m_keyboardHooker->resetActivationModifiers();
    });

    QObject::connect(m_taskbarHooker, &TaskbarWheelHooker::tabWheelEvent,
                     m_widget->taskbarCycler(), &TaskbarWindowCycler::rotate, Qt::QueuedConnection);

    QObject::connect(m_taskbarHooker, &TaskbarWheelHooker::leaveTaskbar,
                     m_widget->taskbarCycler(), &TaskbarWindowCycler::clearOrder, Qt::QueuedConnection);

    qInfo() << "[Main] Hotkey system initialized";
}

bool Application::tryRollback() {
    auto backupDir = UpdateMarker::latestBackupDir();
    if (backupDir.isEmpty()) {
        qWarning() << "[Rollback] No backup directory found";
        return false;
    }

    auto appDir = QCoreApplication::applicationDirPath();
    auto markerPath = UpdateMarker::markerPath();
    auto batPath = QDir::temp().absoluteFilePath("AltTaber_rollback.bat");

    QFile bat(batPath);
    if (!bat.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QString content = QString(
        "@echo off\r\n"
        "timeout /t 3 /nobreak >nul\r\n"
        "xcopy \"%1\\*.*\" \"%2\\\" /E /Y /I >nul 2>&1\r\n"
        "if exist \"%3\" del \"%3\" >nul 2>&1\r\n"
        "rmdir /S /Q \"%1\" >nul 2>&1\r\n"
        "start \"\" \"%2\\AltTaber.exe\"\r\n"
        "del \"%~f0\"\r\n"
    ).arg(QDir::toNativeSeparators(backupDir),
          QDir::toNativeSeparators(appDir),
          QDir::toNativeSeparators(markerPath));

    bat.write(content.toUtf8());
    bat.close();

    qWarning() << "[Rollback] Starting rollback from"
               << backupDir << "to" << appDir;

    if (!QProcess::startDetached(batPath)) {
        qWarning() << "[Rollback] Failed to start rollback script";
        return false;
    }

    QMetaObject::invokeMethod(qApp, "quit", Qt::QueuedConnection);
    return true;
}

int Application::run() {
    return QApplication::exec();
}
