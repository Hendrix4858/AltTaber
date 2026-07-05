#include "utils/VirtualDesktopManager.h"
#include <QDebug>
#include <QSettings>
#include <ShlObj.h>
#include <comdef.h>

// IVirtualDesktopManager
// CLSID = {AA509086-5CA9-4C0E-8B97-3E1E8C1F3E5B}
// IID   = {A5CD92FF-29BE-454C-8D04-D82879FB3F1B}

using PFN_IsWindowOnCurrentVirtualDesktop = HRESULT(STDMETHODCALLTYPE*)(HWND, BOOL*);

static const CLSID CLSID_VDM =
    {0xAA509086, 0x5CA9, 0x4C0E, {0x8B, 0x97, 0x3E, 0x1E, 0x8C, 0x1F, 0x3E, 0x5B}};
static const IID IID_IVDM =
    {0xA5CD92FF, 0x29BE, 0x454C, {0x8D, 0x04, 0xD8, 0x28, 0x79, 0xFB, 0x3F, 0x1B}};

VirtualDesktopManager& VirtualDesktopManager::instance() {
    static VirtualDesktopManager inst;
    return inst;
}

VirtualDesktopManager::VirtualDesktopManager()
    : QObject(nullptr) {
    IUnknown* pUnk = nullptr;
    HRESULT hr = CoCreateInstance(
        CLSID_VDM, nullptr, CLSCTX_LOCAL_SERVER,
        IID_IVDM, reinterpret_cast<void**>(&pUnk));

    if (SUCCEEDED(hr) && pUnk) {
        m_mgr = static_cast<void*>(pUnk);
        m_available = true;
        qInfo() << "[VirtualDesktop] IVirtualDesktopManager initialized";
    } else {
        qWarning() << "[VirtualDesktop] IVirtualDesktopManager not available, hr=" << hr;
    }
}

bool VirtualDesktopManager::isWindowOnCurrentDesktop(HWND hwnd) {
    if (!m_available || !m_mgr || !hwnd)
        return true;

    auto* mgr = static_cast<IUnknown*>(m_mgr);

    // Use the vtable directly - IVirtualDesktopManager vtable:
    // [0] QueryInterface, [1] AddRef, [2] Release
    // [3] IsWindowOnCurrentVirtualDesktop
    auto vtable = *reinterpret_cast<void***>(mgr);
    auto func = reinterpret_cast<PFN_IsWindowOnCurrentVirtualDesktop>(vtable[3]);

    BOOL onCurrentDesktop = TRUE;
    HRESULT hr = func(hwnd, &onCurrentDesktop);
    if (FAILED(hr)) {
        qWarning() << "[VirtualDesktop] IsWindowOnCurrentVirtualDesktop failed, hr=" << hr;
        return true;
    }
    return onCurrentDesktop != FALSE;
}

VirtualDesktopScope VirtualDesktopManager::resolveScope(VirtualDesktopScope configScope) {
    if (configScope != VirtualDesktopScope::FollowSystem)
        return configScope;

    int systemFilter = readSystemAltTabFilter();

    // MultiTaskingAltTabFilter registry values (Windows 11):
    //   1 = "Open windows on all desktops"
    //   3 = "Open windows only on the desktop I'm using"
    if (systemFilter == 3)
        return VirtualDesktopScope::CurrentDesktop;

    return VirtualDesktopScope::AllDesktops;
}

int VirtualDesktopManager::readSystemAltTabFilter() {
    QSettings reg(
        "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced",
        QSettings::NativeFormat);
    return reg.value("MultiTaskingAltTabFilter", 1).toInt();
}
