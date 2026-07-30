#ifndef WIN_SWITCHER_VIRTUALDESKTOPMANAGER_H
#define WIN_SWITCHER_VIRTUALDESKTOPMANAGER_H

#include <windows.h>
#include <QObject>

class VirtualDesktopManager : public QObject {
    Q_OBJECT
public:
    VirtualDesktopManager(const VirtualDesktopManager&) = delete;
    VirtualDesktopManager& operator=(const VirtualDesktopManager&) = delete;

    static VirtualDesktopManager& instance();

    bool isAvailable() const { return m_available; }
    bool isWindowOnCurrentDesktop(HWND hwnd);

private:
    VirtualDesktopManager();

    bool m_available = false;
    void* m_mgr = nullptr;
};

#endif //WIN_SWITCHER_VIRTUALDESKTOPMANAGER_H
