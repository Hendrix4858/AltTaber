#ifndef WIN_SWITCHER_SYSTEMTRAY_H
#define WIN_SWITCHER_SYSTEMTRAY_H

#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QSystemTrayIcon>
#include <QActionGroup>
#include <QProcess>
#include <windows.h>
#include <shellapi.h>
#include "Startup.h"
#include "ConfigManager.h"
#include "UpdateDialog.h"
#include "SettingsDialog.h"
#include "ThemeManager.h"

class SystemTray : public QSystemTrayIcon {
    Q_OBJECT
public:
    SystemTray(const SystemTray&) = delete;
    SystemTray& operator=(const SystemTray&) = delete;

    static SystemTray& instance() {
        static SystemTray instance;
        return instance;
    }

    void retranslateMenu() {
        m_actUpdate->setText(tr("Check for Updates"));
        m_actSettings->setText(tr("Settings"));
        m_actPause->setText(tr("Pause"));
        m_actRestartAdmin->setText(IsUserAnAdmin() ? tr("Running as Administrator") : tr("Restart as Administrator"));
        m_startupBaseText = tr("Start with Windows");
        m_menuMonitor->setTitle(tr("Display Monitor"));
        auto actions = m_monitorGroup->actions();
        actions[PrimaryMonitor]->setText(tr("Primary Monitor"));
        actions[MouseMonitor]->setText(tr("Mouse Monitor"));
        m_actQuit->setText(tr("Quit >"));
        updateStartupText();
    }

private:
    explicit SystemTray(QWidget* parent = nullptr) : QSystemTrayIcon(parent) {
        setIcon(QIcon(":/img/icon.ico"));
        setMenu(parent);
        setToolTip(IsUserAnAdmin() ? "AltTaber (admin)" : "AltTaber");
    }

    void applyMenuTheme() {
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

    void updateStartupText() {
        auto text = m_startupBaseText;
        if (IsUserAnAdmin() && !Startup::isOn_reg())
            text += QString::fromUtf8("\xF0\x9F\x94\x91");
        m_actStartup->setText(text);
    }

    void setMenu(QWidget* parent = nullptr) {
        m_menu = new QMenu(parent);
        applyMenuTheme();

        m_actUpdate = new QAction(m_menu);
        m_actSettings = new QAction(m_menu);
        m_actPause = new QAction(m_menu);
        m_actPause->setCheckable(true);
        m_actRestartAdmin = new QAction(m_menu);
        m_actStartup = new QAction(m_menu);
        m_actStartup->setCheckable(true);
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
            QApplication::quit();
        });

        connect(m_actStartup, &QAction::triggered, this, [this](bool checked) {
            Startup::toggle();
            if (Startup::isOn() == checked)
                this->showMessage(tr("auto Startup mode"), checked ? tr("ON \xE2\x9C\x93") : tr("OFF \xC3\x97"));
            else
                this->showMessage(tr("Action Failed"), tr("Failed to change Startup mode"), Warning);
        });

        connect(m_actPause, &QAction::triggered, this, [this](bool checked) {
            cfg().setPaused(checked);
            this->showMessage(tr("Pause"), checked ? tr("ON \xE2\x9C\x93") : tr("OFF \xC3\x97"));
        });

        connect(m_menu, &QMenu::aboutToShow, this, [this] {
            applyMenuTheme();
            m_actStartup->setChecked(Startup::isOn());
            m_actPause->setChecked(cfg().getPaused());
            bool isAdmin = IsUserAnAdmin();
            m_actRestartAdmin->setText(isAdmin ? tr("Running as Administrator") : tr("Restart as Administrator"));
            m_actRestartAdmin->setEnabled(!isAdmin);
            updateStartupText();
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

        connect(m_actQuit, &QAction::triggered, qApp, &QApplication::quit);

        m_menu->addAction(m_actUpdate);
        m_menu->addAction(m_actSettings);
        m_menu->addSeparator();
        m_menu->addAction(m_actPause);
        m_menu->addSeparator();
        m_menu->addAction(m_actRestartAdmin);
        m_menu->addAction(m_actStartup);
        m_menu->addMenu(m_menuMonitor);
        m_menu->addAction(m_actQuit);
        this->setContextMenu(m_menu);

        retranslateMenu();
    }

    QMenu* m_menu = nullptr;
    QAction* m_actUpdate = nullptr;
    QAction* m_actSettings = nullptr;
    QAction* m_actPause = nullptr;
    QAction* m_actRestartAdmin = nullptr;
    QAction* m_actStartup = nullptr;
    QMenu* m_menuMonitor = nullptr;
    QActionGroup* m_monitorGroup = nullptr;
    QAction* m_actQuit = nullptr;
    QString m_startupBaseText;
};

inline SystemTray& sysTray() { return SystemTray::instance(); }

#endif
