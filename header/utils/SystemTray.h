#ifndef WIN_SWITCHER_SYSTEMTRAY_H
#define WIN_SWITCHER_SYSTEMTRAY_H

#include <QSystemTrayIcon>
#include <QString>

class QAction;
class QActionGroup;
class QMenu;

class SystemTray : public QSystemTrayIcon {
    Q_OBJECT
public:
    SystemTray(const SystemTray&) = delete;
    SystemTray& operator=(const SystemTray&) = delete;

    static SystemTray& instance();

    void retranslateMenu();

signals:
    void showRequested();

private:
    explicit SystemTray(QWidget* parent = nullptr);
    void applyMenuTheme();
    void updateStartupText();
    void setMenu(QWidget* parent);

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

#endif //WIN_SWITCHER_SYSTEMTRAY_H
