#include "utils/KeyboardHooker.h"
#include <QDebug>
#include <atomic>
#include "utils/Util.h"

namespace {
    constexpr UINT WM_APP_REC_KEY = WM_APP + 0x100;

    struct ModifierState {
        bool ctrl = false;
        bool shift = false;
        bool alt = false;
        bool meta = false;
    };

    static ModifierState s_modState;
    static inline Qt::KeyboardModifiers toQtModifiers(const ModifierState& ms) {
        Qt::KeyboardModifiers m;
        if (ms.ctrl)  m |= Qt::ControlModifier;
        if (ms.shift) m |= Qt::ShiftModifier;
        if (ms.alt)   m |= Qt::AltModifier;
        if (ms.meta)  m |= Qt::MetaModifier;
        return m;
    }

    static void updateModifierState(WPARAM wParam, DWORD vkCode) {
        bool down = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
        switch (vkCode) {
            case VK_CONTROL:
            case VK_LCONTROL:
            case VK_RCONTROL:  s_modState.ctrl  = down; break;
            case VK_SHIFT:
            case VK_LSHIFT:
            case VK_RSHIFT:    s_modState.shift = down; break;
            case VK_MENU:
            case VK_LMENU:
            case VK_RMENU:     s_modState.alt   = down; break;
            case VK_LWIN:
            case VK_RWIN:      s_modState.meta  = down; break;
        }
    }

    void snapshotModifiersFromOS(ModifierState& ms) {
        ms.ctrl  = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        ms.shift = (GetAsyncKeyState(VK_SHIFT)   & 0x8000) != 0;
        ms.alt   = (GetAsyncKeyState(VK_MENU)    & 0x8000) != 0;
        ms.meta  = ((GetAsyncKeyState(VK_LWIN) | GetAsyncKeyState(VK_RWIN)) & 0x8000) != 0;
    }

    KeyboardHooker* s_instance = nullptr;
    std::atomic<bool> s_recordingActive = false;

    struct RawKeyRecord {
        quint32 vkCode = 0;
        quint32 scanCode = 0;
        DWORD flags = 0;
        Qt::KeyboardModifiers modifiers = Qt::NoModifier;
        std::atomic<bool> ready{false};
    };
    RawKeyRecord s_keyRecord;
    HWND s_recorderHwnd = nullptr;

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

        auto* pKB = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);

        // Maintain self-owned modifier state (does NOT depend on GetAsyncKeyState timing)
        updateModifierState(wParam, pKB->vkCode);

        if (s_instance->m_paused)
            return CallNextHookEx(nullptr, nCode, wParam, lParam);

        if (pKB->flags & LLKHF_INJECTED) {
            qDebug() << "[KeyHook] Injected event ignored, vkCode=" << pKB->vkCode;
            return CallNextHookEx(nullptr, nCode, wParam, lParam);
        }

        Qt::KeyboardModifiers mods = toQtModifiers(s_modState);

        // Recording pipeline: route key to recorder via PostMessage
        if (s_recorderHwnd && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
            s_keyRecord.vkCode = pKB->vkCode;
            s_keyRecord.scanCode = pKB->scanCode;
            s_keyRecord.flags = pKB->flags;
            s_keyRecord.modifiers = mods;
            s_keyRecord.ready.store(true, std::memory_order_release);
            qDebug() << "[RecordRaw] post vk=" << pKB->vkCode
                     << "sc=" << pKB->scanCode << "flags=" << pKB->flags
                     << "mods=" << mods;
            PostMessageW(s_recorderHwnd, WM_APP_REC_KEY, 0, 0);
            return 1;
        }

        if (s_recordingActive)
            return CallNextHookEx(nullptr, nCode, wParam, lParam);

        if (wParam == WM_SYSKEYDOWN || wParam == WM_KEYDOWN) {
            if (!s_instance) return CallNextHookEx(nullptr, nCode, wParam, lParam);

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
                    bool match = binding.matchesPhysical(pKB->vkCode, pKB->scanCode,
                                          (pKB->flags & LLKHF_EXTENDED) != 0, mods);
                    if (match) {
                        qInfo() << "[KeyHook] emit" << hotkeyActionName(it.key());
                        emit s_instance->hotkeyTriggered(it.key(), mods);
                        return 1;
                    }
                }
            }
            if (checkedCount == 0)
                qDebug() << "[KeyHook] No global actions have bindings registered";

            // Legacy convenience: Alt+Arrow routing when overlay visible but not foreground
            // Uses overlay bindings for vkCode match (Alt is already held)
            if (s_modState.alt) {
                bool overlayVisible = IsWindowVisible(s_instance->m_ownerHwnd);
                bool isForeground = s_instance->m_ownerHwnd == GetForegroundWindow();
                if (overlayVisible && !isForeground) {
                    static const HotkeyAction legacyActions[] = {
                        HotkeyAction::CycleForward,
                        HotkeyAction::CycleBackward,
                        HotkeyAction::MoveSelectionUp,
                        HotkeyAction::MoveSelectionDown,
                    };
                    for (auto action : legacyActions) {
                        auto it = s_instance->m_bindings.find(action);
                        if (it == s_instance->m_bindings.end()) continue;
                        for (const auto& b : it.value()) {
                            if (b.vkCode == pKB->vkCode) {
                                qDebug() << "[KeyHook] Alt+routing ->"
                                         << hotkeyActionName(action);
                                emit s_instance->hotkeyTriggered(action, mods);
                                return 1;
                            }
                        }
                    }
                }
            }
        } else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
            if (pKB->vkCode == VK_LMENU || pKB->vkCode == VK_RMENU) {
                qDebug() << "[KeyHook] Alt UP";
                emit s_instance->altReleased();
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

void KeyboardHooker::setRecordingTarget(HWND hwnd) {
    s_recorderHwnd = hwnd;
    s_keyRecord.ready.store(false, std::memory_order_release);
    snapshotModifiersFromOS(s_modState);
}

void KeyboardHooker::clearRecordingTarget() {
    s_recorderHwnd = nullptr;
    s_keyRecord.ready.store(false, std::memory_order_release);
}

UINT KeyboardHooker::recordingMessageId() {
    return WM_APP_REC_KEY;
}

bool KeyboardHooker::tryTakeRecordedKey(quint32& vk, quint32& scanCode,
                                         DWORD& flags, Qt::KeyboardModifiers& mods) {
    if (!s_keyRecord.ready.load(std::memory_order_acquire))
        return false;
    vk = s_keyRecord.vkCode;
    scanCode = s_keyRecord.scanCode;
    flags = s_keyRecord.flags;
    mods = s_keyRecord.modifiers;
    s_keyRecord.ready.store(false, std::memory_order_release);
    return true;
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
