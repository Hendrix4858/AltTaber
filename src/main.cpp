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
#include "utils/ConfigManager.h"
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

    QObject::connect(&a, &QApplication::aboutToQuit, []() {
        unhookWinEvent();
        Util::Logger::shutdown();
    });

    // Load hotkey bindings
    auto bindings = cfg().getHotkeyBindings();

    // Global scope actions that need defaults if empty
    auto& showBinds = bindings[HotkeyAction::ShowSwitcher];
    bool hasAltTab = false;
    for (const auto& b : showBinds) {
        if (b.vkCode == VK_TAB && b.modifiers == Qt::AltModifier) {
            hasAltTab = true;
            break;
        }
    }
    if (!hasAltTab)
        showBinds.append(HotkeyBinding::fromString("Alt+Tab"));
    auto& enterBinds = bindings[HotkeyAction::EnterGroupMode];
    if (enterBinds.isEmpty())
        enterBinds.append(HotkeyBinding::fromString("Alt+Grave"));
    auto& cfBinds = bindings[HotkeyAction::CycleForward];
    if (cfBinds.isEmpty())
        cfBinds.append(HotkeyBinding::fromString("Tab"));
    auto& cbBinds = bindings[HotkeyAction::CycleBackward];
    if (cbBinds.isEmpty())
        cbBinds.append(HotkeyBinding::fromString("Shift+Tab"));
    auto& asBinds = bindings[HotkeyAction::ActivateSelected];
    if (asBinds.isEmpty())
        asBinds.append(HotkeyBinding::fromString("Enter"));
    auto& dsBinds = bindings[HotkeyAction::DismissSwitcher];
    if (dsBinds.isEmpty())
        dsBinds.append(HotkeyBinding::fromString("Escape"));
    auto& muBinds = bindings[HotkeyAction::MoveSelectionUp];
    if (muBinds.isEmpty())
        muBinds.append(HotkeyBinding::fromString("Up"));
    auto& mdBinds = bindings[HotkeyAction::MoveSelectionDown];
    if (mdBinds.isEmpty())
        mdBinds.append(HotkeyBinding::fromString("Down"));

    KeyboardHooker kbHooker((HWND) winSwitcher->winId());
    kbHooker.updateBindings(bindings);
    winSwitcher->updateOverlayBindings(bindings);

    QObject::connect(&kbHooker, &KeyboardHooker::hotkeyTriggered,
                     winSwitcher, &Widget::handleGlobalAction, Qt::QueuedConnection);
    QObject::connect(&kbHooker, &KeyboardHooker::altReleased, winSwitcher,
                     [winSwitcher]() {
        auto event = new QKeyEvent(QEvent::KeyRelease, Qt::Key_Alt, Qt::NoModifier);
        QApplication::postEvent(winSwitcher, event);
    }, Qt::QueuedConnection);

    // Re-inject bindings when config is saved (settings dialog or notepad edit)
    QObject::connect(&cfg(), &ConfigManager::configEdited, &a, [&kbHooker, winSwitcher]() {
        qInfo() << "[Config] configEdited -> re-injecting hotkey bindings";
        auto b = cfg().getHotkeyBindings();

        // Apply defaults if missing (same as startup code above)
        auto& showBinds = b[HotkeyAction::ShowSwitcher];
        bool hasAltTab = false;
        for (const auto& bind : showBinds) {
            if (bind.vkCode == VK_TAB && bind.modifiers == Qt::AltModifier) {
                hasAltTab = true;
                break;
            }
        }
        if (!hasAltTab)
            showBinds.append(HotkeyBinding::fromString("Alt+Tab"));
        if (b[HotkeyAction::EnterGroupMode].isEmpty())
            b[HotkeyAction::EnterGroupMode].append(HotkeyBinding::fromString("Alt+Grave"));
        if (b[HotkeyAction::CycleForward].isEmpty())
            b[HotkeyAction::CycleForward].append(HotkeyBinding::fromString("Tab"));
        if (b[HotkeyAction::CycleBackward].isEmpty())
            b[HotkeyAction::CycleBackward].append(HotkeyBinding::fromString("Shift+Tab"));
        if (b[HotkeyAction::ActivateSelected].isEmpty())
            b[HotkeyAction::ActivateSelected].append(HotkeyBinding::fromString("Enter"));
        if (b[HotkeyAction::DismissSwitcher].isEmpty())
            b[HotkeyAction::DismissSwitcher].append(HotkeyBinding::fromString("Escape"));
        if (b[HotkeyAction::MoveSelectionUp].isEmpty())
            b[HotkeyAction::MoveSelectionUp].append(HotkeyBinding::fromString("Up"));
        if (b[HotkeyAction::MoveSelectionDown].isEmpty())
            b[HotkeyAction::MoveSelectionDown].append(HotkeyBinding::fromString("Down"));

        kbHooker.updateBindings(b);
        winSwitcher->updateOverlayBindings(b);
    });

    TaskbarWheelHooker tbHooker;
    QObject::connect(&tbHooker, &TaskbarWheelHooker::tabWheelEvent,
                     winSwitcher, &Widget::rotateTaskbarWindowInGroup, Qt::QueuedConnection);
    QObject::connect(&tbHooker, &TaskbarWheelHooker::leaveTaskbar,
                     winSwitcher, &Widget::clearGroupWindowOrder, Qt::QueuedConnection);

    setWinEventHook([winSwitcher](DWORD event, HWND hwnd) {
        if (event == EVENT_SYSTEM_FOREGROUND) {
            winSwitcher->notifyForegroundChanged(hwnd);

            if (IsWindowVisible((HWND)winSwitcher->winId()) && hwnd != (HWND)winSwitcher->winId()
                && !Util::isKeyPressed(VK_MENU)
                && winSwitcher->overlayState() != Widget::OverlayState::Visible) {
                QMetaObject::invokeMethod(winSwitcher, [winSwitcher]() {
                    winSwitcher->hide();
                }, Qt::QueuedConnection);
            }

            auto className = Util::getClassName(hwnd);
            if (hwnd == GetForegroundWindow() && Util::isKeyPressed(VK_MENU) &&
                (className == "ForegroundStaging") &&
                winSwitcher->overlayState() != Widget::OverlayState::Visible) {
                qInfo() << "Task switcher detected (fallback)" << className;
                winSwitcher->requestShow();
            }
        }
    });

    QTimer::singleShot(0, winSwitcher, &Widget::warmupCache);

    return QApplication::exec();
}
