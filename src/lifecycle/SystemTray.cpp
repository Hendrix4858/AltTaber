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
    m_actUpdate->setText(tr("Check for Updates"));
    m_actSettings->setText(tr("Settings"));
    m_actPause->setText(tr("Pause"));
    m_actRestartAdmin->setText(IsUserAnAdmin() ? tr("Running as Administrator") : tr("Restart as Administrator"));
    m_menuMonitor->setTitle(tr("Display Monitor"));
    auto actions = m_monitorGroup->actions();
    actions[PrimaryMonitor]->setText(tr("Primary Monitor"));
    actions[MouseMonitor]->setText(tr("Mouse Monitor"));
    m_actQuit->setText(tr("Quit >"));
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

    m_actUpdate = new QAction(m_menu);
    m_actSettings = new QAction(m_menu);
    m_actPause = new QAction(m_menu);
    m_actPause->setCheckable(true);
    m_actRestartAdmin = new QAction(m_menu);
    m_menuMonitor = new QMenu(m_menu);
    m_actQuit = new QAction(m_menu);

    connect(m_actUpdate, &QAction::triggered, this, [] {
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

    connect(m_actSettings, &QAction::triggered, this, [] {
        static SettingsDialog* dlg = nullptr;
        if (!dlg) {
            dlg = new SettingsDialog;
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            QObject::connect(dlg, &QObject::destroyed, [] { dlg = nullptr; });
        }
        dlg->show();
        dlg->raise();
        dlg->activateWindow();
    });

    connect(m_actRestartAdmin, &QAction::triggered, this, [] {
        QString appPath = QApplication::applicationFilePath();
        ShellExecuteW(nullptr, L"runas", (LPCWSTR)appPath.utf16(), nullptr, nullptr, SW_SHOWNORMAL);
        QuitReason::markIntentional();
        QApplication::quit();
    });

    connect(m_actPause, &QAction::triggered, this, [this](bool checked) {
        cfg().setPaused(checked);
        this->showMessage(tr("Pause"), checked ? tr("ON \xE2\x9C\x93") : tr("OFF \xC3\x97"));
    });

    connect(m_menu, &QMenu::aboutToShow, this, [this] {
        applyMenuTheme();
        m_actPause->setChecked(cfg().getPaused());
        bool isAdmin = IsUserAnAdmin();
        m_actRestartAdmin->setText(isAdmin ? tr("Running as Administrator") : tr("Restart as Administrator"));
        m_actRestartAdmin->setEnabled(!isAdmin);
    });

    // Display Monitor submenu
    {
        m_monitorGroup = new QActionGroup(m_menuMonitor);
        auto* actPrimary = m_monitorGroup->addAction(QString());
        actPrimary->setData(PrimaryMonitor);
        auto* actMouse = m_monitorGroup->addAction(QString());
        actMouse->setData(MouseMonitor);

        const auto actions = m_monitorGroup->actions();
        for (auto* act : actions)
            act->setCheckable(true);

        Q_ASSERT(actions.size() == DisplayMonitor::EnumCount);
        m_menuMonitor->addActions(actions);

        connect(m_menuMonitor, &QMenu::aboutToShow, this, [this] {
            const auto actions = m_monitorGroup->actions();
            actions[cfg().getDisplayMonitor()]->setChecked(true);
        });

        connect(m_monitorGroup, &QActionGroup::triggered, this, [this](QAction* act) {
            auto monitor = static_cast<DisplayMonitor>(act->data().toInt());
            cfg().setDisplayMonitor(monitor);
            this->showMessage(tr("Display Monitor Changed"), act->text());
        });
    }

    connect(m_actQuit, &QAction::triggered, this, [] {
        QuitReason::markIntentional();
        QApplication::quit();
    });

    m_menu->addAction(m_actUpdate);
    m_menu->addAction(m_actSettings);
    m_menu->addSeparator();
    m_menu->addAction(m_actPause);
    m_menu->addSeparator();
    m_menu->addAction(m_actRestartAdmin);
    m_menu->addMenu(m_menuMonitor);
    m_menu->addAction(m_actQuit);
    this->setContextMenu(m_menu);

    retranslateMenu();
}
