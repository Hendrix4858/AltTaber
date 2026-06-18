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
    void setMenu(QWidget* parent);

    QMenu* m_menu = nullptr;
    QAction* m_updateAction = nullptr;
    QAction* m_settingsAction = nullptr;
    QAction* m_pauseAction = nullptr;
    QAction* m_restartAction = nullptr;
    QAction* m_restartAdminAction = nullptr;
    QMenu* m_monitorMenu = nullptr;
    QActionGroup* m_monitorGroup = nullptr;
    QAction* m_quitAction = nullptr;
};

inline SystemTray& sysTray() { return SystemTray::instance(); }

#endif //WIN_SWITCHER_SYSTEMTRAY_H
