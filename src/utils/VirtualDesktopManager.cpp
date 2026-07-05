#include "utils/VirtualDesktopManager.h"
#include <QDebug>
#include <ShlObj.h>

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
