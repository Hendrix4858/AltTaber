#include "lifecycle/SystemTray.h"
#include "core/ThemeManager.h"
#include "core/ConfigManager.h"
#include "core/QuitReason.h"
#include "UpdateDialog.h"
#include "SettingsDialog.h"
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <windows.h>
#include <shellapi.h>
#include <shlobj_core.h>

SystemTray& SystemTray::instance() {
    static SystemTray instance;
    return instance;
}

SystemTray::SystemTray(QWidget* parent)
    : QSystemTrayIcon(parent) {
    setIcon(QIcon(":/img/icon.ico"));
    setMenu(parent);
    setToolTip(IsUserAnAdmin() ? "AltTaber (admin)" : "AltTaber");

    connect(this, &QSystemTrayIcon::activated, this, [this](ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
            emit showRequested();
    });
}

void SystemTray::retranslateMenu() {
    m_updateAction->setText(tr("Check for Updates"));
    m_settingsAction->setText(tr("Settings"));
    m_pauseAction->setText(tr("Pause"));
    m_restartAction->setText(tr("Restart"));
    m_restartAdminAction->setText(IsUserAnAdmin() ? tr("Running as Administrator") : tr("Restart as Administrator"));
    m_monitorMenu->setTitle(tr("Display Monitor"));
    auto actions = m_monitorGroup->actions();
    actions[PrimaryMonitor]->setText(tr("Primary Monitor"));
    actions[MouseMonitor]->setText(tr("Mouse Monitor"));
    m_quitAction->setText(tr("Quit >"));
}

void SystemTray::applyMenuTheme() {
    const auto& c = ThemeManager::current();
    m_menu->setStyleSheet(
        QString("QMenu{"
            "background-color:%1;"
            "color:%2;"
            "border:1px solid %3;"
            "}"
            "QMenu::item:selected{ background-color:%4; }")
        .arg(c.trayBg.name(), c.trayText.name(), c.trayBorder.name(), c.traySelected.name()));
}

void SystemTray::setMenu(QWidget* parent) {
    m_menu = new QMenu(parent);
    applyMenuTheme();

    m_updateAction = new QAction(m_menu);
    m_settingsAction = new QAction(m_menu);
    m_pauseAction = new QAction(m_menu);
    m_pauseAction->setCheckable(true);
    m_restartAction = new QAction(m_menu);
    m_restartAdminAction = new QAction(m_menu);
    m_monitorMenu = new QMenu(m_menu);
    m_quitAction = new QAction(m_menu);

    connect(m_updateAction, &QAction::triggered, this, [] {
        static UpdateDialog* dlg = nullptr;
        if (!dlg) {
            dlg = new UpdateDialog;
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            QObject::connect(dlg, &QObject::destroyed, [] { dlg = nullptr; });
        }
        dlg->show();
        dlg->raise();
        dlg->activateWindow();
    });

    connect(m_settingsAction, &QAction::triggered, this, [] {
        static SettingsDialog* dlg = nullptr;
        if (!dlg) {
            dlg = new SettingsDialog(&cfg());
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            QObject::connect(dlg, &QObject::destroyed, [] { dlg = nullptr; });
        }
        dlg->show();
        dlg->raise();
        dlg->activateWindow();
    });

    connect(m_restartAdminAction, &QAction::triggered, this, [] {
        QString appPath = QApplication::applicationFilePath();
        ShellExecuteW(nullptr, L"runas", (LPCWSTR)appPath.utf16(), nullptr, nullptr, SW_SHOWNORMAL);
        QuitReason::markIntentional();
        QApplication::quit();
    });

    connect(m_pauseAction, &QAction::triggered, this, [this](bool checked) {
        cfg().setPaused(checked);
        this->showMessage(tr("Pause"), checked ? tr("ON \xE2\x9C\x93") : tr("OFF \xC3\x97"));
    });

    connect(m_restartAction, &QAction::triggered, this, [] {
        QString appPath = QApplication::applicationFilePath();
        ShellExecuteW(nullptr, nullptr, (LPCWSTR)appPath.utf16(), nullptr, nullptr, SW_SHOWNORMAL);
        QuitReason::markIntentional();
        QApplication::quit();
    });

    connect(m_menu, &QMenu::aboutToShow, this, [this] {
        applyMenuTheme();
        m_pauseAction->setChecked(cfg().getPaused());
        bool isAdmin = IsUserAnAdmin();
        m_restartAdminAction->setText(isAdmin ? tr("Running as Administrator") : tr("Restart as Administrator"));
        m_restartAdminAction->setEnabled(!isAdmin);
    });

    // Display Monitor submenu
    {
        m_monitorGroup = new QActionGroup(m_monitorMenu);
        auto* actPrimary = m_monitorGroup->addAction(QString());
        actPrimary->setData(PrimaryMonitor);
        auto* actMouse = m_monitorGroup->addAction(QString());
        actMouse->setData(MouseMonitor);

        const auto actions = m_monitorGroup->actions();
        for (auto* act : actions)
            act->setCheckable(true);

        Q_ASSERT(actions.size() == DisplayMonitorCount);
        m_monitorMenu->addActions(actions);

        connect(m_monitorMenu, &QMenu::aboutToShow, this, [this] {
            const auto actions = m_monitorGroup->actions();
            actions[cfg().getDisplayMonitor()]->setChecked(true);
        });

        connect(m_monitorGroup, &QActionGroup::triggered, this, [this](QAction* act) {
            auto monitor = static_cast<DisplayMonitor>(act->data().toInt());
            cfg().setDisplayMonitor(monitor);
            this->showMessage(tr("Display Monitor Changed"), act->text());
        });
    }

    connect(m_quitAction, &QAction::triggered, this, [] {
        QuitReason::markIntentional();
        QApplication::quit();
    });

    m_menu->addAction(m_updateAction);
    m_menu->addAction(m_settingsAction);
    m_menu->addSeparator();
    m_menu->addAction(m_pauseAction);
    m_menu->addAction(m_restartAction);
    m_menu->addSeparator();
    m_menu->addAction(m_restartAdminAction);
    m_menu->addMenu(m_monitorMenu);
    m_menu->addAction(m_quitAction);
    this->setContextMenu(m_menu);

    retranslateMenu();
}
