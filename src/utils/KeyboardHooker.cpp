#include "utils/KeyboardHooker.h"
#include <QDebug>
#include "utils/Util.h"

namespace {
    KeyboardHooker* s_instance = nullptr;
    HWND s_ownerHwnd = nullptr;
}

LRESULT keyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        if (wParam == WM_SYSKEYDOWN || wParam == WM_KEYDOWN) {
            auto* pKeyBoard = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
            bool isAltPressed = Util::isKeyPressed(VK_MENU);

            if (isAltPressed && s_instance) {
                if (pKeyBoard->vkCode == VK_TAB) {
                    if (s_ownerHwnd != GetForegroundWindow()) {
                        emit s_instance->requestShow();
                    } else {
                        auto shiftModifier = Util::isKeyPressed(VK_SHIFT) ? Qt::ShiftModifier : Qt::NoModifier;
                        emit s_instance->altTabPressed(shiftModifier);
                    }
                    return 1;
                } else if (pKeyBoard->vkCode == VK_OEM_3) {
                    qDebug() << "[Alt+`] Detected, isAltPressed:" << isAltPressed << "foreground:" << Util::getWindowTitle(GetForegroundWindow());
                    auto shiftModifier = Util::isKeyPressed(VK_SHIFT) ? Qt::ShiftModifier : Qt::NoModifier;
                    emit s_instance->altGravePressed(shiftModifier);
                    return 1;
                }
            }
        } else if (wParam == WM_KEYUP) {
            auto* pKeyBoard = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
            if (pKeyBoard->vkCode == VK_LMENU && s_instance) {
                emit s_instance->altReleased();
            }
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

KeyboardHooker::KeyboardHooker(HWND ownerHwnd, QObject* parent)
    : QObject(parent) {
    if (s_instance) {
        qWarning() << "Only one KeyboardHooker can be installed!";
        return;
    }
    h_keyboard = SetWindowsHookEx(WH_KEYBOARD_LL, (HOOKPROC) keyboardProc, GetModuleHandle(nullptr), 0);
    if (!h_keyboard) {
        qWarning() << "Failed to install h_keyboard!";
        return;
    }
    if (!ownerHwnd) {
        qWarning() << "Owner HWND is null!";
        return;
    }
    s_instance = this;
    s_ownerHwnd = ownerHwnd;
    qInfo() << "KeyboardHooker installed";
}

KeyboardHooker::~KeyboardHooker() {
    if (!h_keyboard) return;
    UnhookWindowsHookEx(h_keyboard);
    s_instance = nullptr;
    s_ownerHwnd = nullptr;
    qDebug() << "KeyboardHooker uninstalled";
}
