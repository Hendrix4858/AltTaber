#include "utils/KeyboardHooker.h"
#include <QDebug>
#include <atomic>
#include "utils/Util.h"

namespace {
    KeyboardHooker* s_instance = nullptr;
    std::atomic<bool> s_recordingActive = false;

    bool isGlobalAction(HotkeyAction action) {
        switch (action) {
            case HotkeyAction::ShowSwitcher:
            case HotkeyAction::EnterGroupMode:
            case HotkeyAction::TogglePause:
                return true;
            default:
                return false;
        }
    }
}

LRESULT CALLBACK keyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        if (!s_instance) return CallNextHookEx(nullptr, nCode, wParam, lParam);
        if (s_instance->m_paused)
            return CallNextHookEx(nullptr, nCode, wParam, lParam);
        if (s_recordingActive)
            return CallNextHookEx(nullptr, nCode, wParam, lParam);

        auto* pKB = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);

        if (pKB->flags & LLKHF_INJECTED) {
            qDebug() << "[KeyHook] Injected event ignored, vkCode=" << pKB->vkCode;
            return CallNextHookEx(nullptr, nCode, wParam, lParam);
        }

        if (wParam == WM_SYSKEYDOWN || wParam == WM_KEYDOWN) {
            if (!s_instance) return CallNextHookEx(nullptr, nCode, wParam, lParam);

            // Track Alt key state
            if (pKB->vkCode == VK_LMENU || pKB->vkCode == VK_RMENU) {
                qDebug() << "[KeyHook] Alt DOWN";
                s_instance->m_altDown = true;
            }

            // Build current modifier state
            Qt::KeyboardModifiers mods;
            if (s_instance->m_altDown || (GetAsyncKeyState(VK_MENU) & 0x8000))
                mods |= Qt::AltModifier;
            if (GetAsyncKeyState(VK_CONTROL) & 0x8000)
                mods |= Qt::ControlModifier;
            if (GetAsyncKeyState(VK_SHIFT) & 0x8000)
                mods |= Qt::ShiftModifier;
            if ((GetAsyncKeyState(VK_LWIN) | GetAsyncKeyState(VK_RWIN)) & 0x8000)
                mods |= Qt::MetaModifier;

            QString keyName = HotkeyStrings::vkCodeToString(pKB->vkCode);
            qDebug() << "[KeyHook] KeyDown: vk=" << pKB->vkCode
                     << "key=" << keyName << "mods=" << mods;

            // Check ALL global action bindings (not just when Alt is held)
            int checkedCount = 0;
            for (auto it = s_instance->m_bindings.begin();
                 it != s_instance->m_bindings.end(); ++it) {
                if (!isGlobalAction(it.key())) continue;

                for (const auto& binding : it.value()) {
                    ++checkedCount;
                    if (binding.vkCode == pKB->vkCode
                        && (mods & binding.modifiers) == binding.modifiers) {
                        qInfo() << "[KeyHook] emit" << hotkeyActionName(it.key());
                        emit s_instance->hotkeyTriggered(it.key(), mods);
                        return 1;
                    }
                }
            }
            if (checkedCount == 0)
                qDebug() << "[KeyHook] No global actions have bindings registered";

            // Legacy convenience: Alt+Arrow routing when overlay not in foreground
            // (only when Alt is held, for backward compatibility)
            if (s_instance->m_altDown || (GetAsyncKeyState(VK_MENU) & 0x8000)) {
                bool overlayVisible = IsWindowVisible(s_instance->m_ownerHwnd);
                bool isForeground = s_instance->m_ownerHwnd == GetForegroundWindow();
                if (overlayVisible && !isForeground) {
                    HotkeyAction arrowAction;
                    switch (pKB->vkCode) {
                        case VK_LEFT:  arrowAction = HotkeyAction::CycleBackward; break;
                        case VK_RIGHT: arrowAction = HotkeyAction::CycleForward;  break;
                        case VK_UP:    arrowAction = HotkeyAction::MoveSelectionUp;   break;
                        case VK_DOWN:  arrowAction = HotkeyAction::MoveSelectionDown;  break;
                        default: arrowAction = HotkeyAction::ShowSwitcher;
                    }
                    if (arrowAction != HotkeyAction::ShowSwitcher) {
                        qDebug() << "[KeyHook] Alt+Arrow routing ->"
                                 << hotkeyActionName(arrowAction);
                        emit s_instance->hotkeyTriggered(arrowAction, mods);
                        return 1;
                    }
                }
            }
        } else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
            if (pKB->vkCode == VK_LMENU || pKB->vkCode == VK_RMENU) {
                QString which = (pKB->vkCode == VK_LMENU) ? "L" : "R";
                qDebug() << "[KeyHook]" << which << "Alt UP";
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
        qCritical() << "Only one KeyboardHooker can be installed!";
        return;
    }
    h_keyboard = SetWindowsHookEx(WH_KEYBOARD_LL, (HOOKPROC) keyboardProc, GetModuleHandle(nullptr), 0);
    if (!h_keyboard) {
        qCritical() << "Failed to install h_keyboard!";
        return;
    }
    if (!ownerHwnd) {
        qCritical() << "Owner HWND is null!";
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

void KeyboardHooker::setRecordingActive(bool active) {
    s_recordingActive = active;
}

void KeyboardHooker::setPaused(bool paused) {
    m_paused = paused;
}

void KeyboardHooker::updateBindings(const HotkeyBindings& bindings) {
    m_bindings = bindings;
    qDebug() << "[KeyHook] Bindings updated,"
             << m_bindings.size() << "actions registered";
    for (auto it = m_bindings.begin(); it != m_bindings.end(); ++it) {
        QStringList strs;
        for (const auto& b : it.value())
            strs << b.toString();
        qDebug() << "[KeyHook]  " << hotkeyActionName(it.key()) << ":" << strs.join(", ");
    }
}
