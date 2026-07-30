#include "hook/winEventHook.h"
#include "core/ConfigManager.h"
#include "utils/Util.h"

static HWINEVENTHOOK handler = nullptr;
static WinEventCallback callback = nullptr;

void CALLBACK WinEventProc(HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD dwEventThread,
                           DWORD dwmsEventTime) {
    if (idObject != OBJID_WINDOW)
        return;

    if (cfg().getPaused())
        return;

    if (callback)
        callback(event, hwnd);
}

bool setWinEventHook(WinEventCallback _callback) {
    if (handler)
        return false;

    ::callback = std::move(_callback);

    handler = SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND,
        EVENT_OBJECT_HIDE,
        nullptr,
        WinEventProc,
        0,
        0,
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS
    );

    qInfo() << "Set WinEventHook";

    return handler != nullptr;
}

void unhookWinEvent() {
    if (!handler) return;

    callback = nullptr;
    UnhookWinEvent(handler);
    handler = nullptr;
}
