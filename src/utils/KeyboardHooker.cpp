#include "utils/KeyboardHooker.h"
#include <QDebug>
#include "utils/Util.h"
#include "utils/ConfigManager.h"

namespace {
    KeyboardHooker* s_instance = nullptr;
}

LRESULT CALLBACK keyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        if (cfg().getPaused())
            return CallNextHookEx(nullptr, nCode, wParam, lParam);

        auto* pKB = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);

        // 过滤 SendInput 注入的虚假按键事件，防止干扰 Alt 状态追踪
        if (pKB->flags & LLKHF_INJECTED) {
            qDebug() << "[KeyHook] Injected event ignored, vkCode=" << pKB->vkCode;
            return CallNextHookEx(nullptr, nCode, wParam, lParam);
        }

        if (wParam == WM_SYSKEYDOWN || wParam == WM_KEYDOWN) {
            // Track Alt key state internally
            if (pKB->vkCode == VK_LMENU) {
                qDebug() << "[KeyHook] LAlt DOWN";
                if (s_instance) s_instance->m_altDown = true;
            } else if (pKB->vkCode == VK_RMENU) {
                qDebug() << "[KeyHook] RAlt DOWN";
                if (s_instance) s_instance->m_altDown = true;
            }

            if (s_instance && s_instance->m_altDown) {
                if (pKB->vkCode == VK_TAB) {
                    qDebug() << "[KeyHook] Alt+Tab, foreground owner:"
                             << (s_instance->m_ownerHwnd == GetForegroundWindow());
                    if (s_instance->m_ownerHwnd != GetForegroundWindow()) {
                        qDebug() << "[KeyHook] -> requestShow";
                        emit s_instance->requestShow();
                    } else {
                        auto shiftModifier = Util::isKeyPressed(VK_SHIFT) ? Qt::ShiftModifier : Qt::NoModifier;
                        qDebug() << "[KeyHook] -> altTabPressed, shift:" << (bool)shiftModifier;
                        emit s_instance->altTabPressed(shiftModifier);
                    }
                    return 1;
                } else if (pKB->vkCode == VK_OEM_3) {
                    qDebug() << "[KeyHook] Alt+` detected, m_altDown:" << s_instance->m_altDown;
                    auto shiftModifier = Util::isKeyPressed(VK_SHIFT) ? Qt::ShiftModifier : Qt::NoModifier;
                    emit s_instance->altGravePressed(shiftModifier);
                    return 1;
                }
            }
        } else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
            if (pKB->vkCode == VK_LMENU || pKB->vkCode == VK_RMENU) {
                QString which = (pKB->vkCode == VK_LMENU) ? "L" : "R";
                qDebug() << "[KeyHook]" << which << "Alt UP, wParam:" << (wParam == WM_SYSKEYUP ? "SYSKEYUP" : "KEYUP");
                if (s_instance) {
                    s_instance->m_altDown = false;
                    emit s_instance->altReleased();
                }
            }
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

KeyboardHooker::KeyboardHooker(HWND ownerHwnd, QObject* parent)
    : QObject(parent), m_ownerHwnd(ownerHwnd) {
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
    qInfo() << "KeyboardHooker installed";
}

KeyboardHooker::~KeyboardHooker() {
    if (!h_keyboard) return;
    UnhookWindowsHookEx(h_keyboard);
    s_instance = nullptr;
    qDebug() << "KeyboardHooker uninstalled";
}
