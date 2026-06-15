#include "hook/winEventHook.h"
#include "core/ConfigManager.h"
#include "utils/Util.h"
#include <QDebug>
#include <QTime>

static HWINEVENTHOOK handler = nullptr;
static WinEventCallback callback = nullptr;

void CALLBACK WinEventProc(HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD dwEventThread,
                           DWORD dwmsEventTime) {
    if (idObject != OBJID_WINDOW)
        return;

    if (cfg().getPaused())
        return;

    // Diagnostic logging for interesting window events
    const char* eventName = nullptr;
    switch (event) {
        case EVENT_SYSTEM_FOREGROUND: eventName = "EVENT_SYSTEM_FOREGROUND"; break;
        case EVENT_OBJECT_SHOW:       eventName = "EVENT_OBJECT_SHOW"; break;
        case EVENT_OBJECT_HIDE:       eventName = "EVENT_OBJECT_HIDE"; break;
    }
    if (eventName) {
        WCHAR className[256] = {};
        GetClassNameW(hwnd, className, 256);
        auto title = Util::getWindowTitle(hwnd);
        qDebug() << "[WinEvent]" << eventName
                << "hwnd=" << Qt::hex << hwnd << Qt::dec
                << "class=" << QString::fromWCharArray(className)
                << "title=" << title;
    }

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

    qDebug() << "Unhook win event.";
}
