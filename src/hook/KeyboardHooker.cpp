#include <QDebug>
#include <QTimer>
#include "hook/KeyboardHooker.h"
#include "utils/Util.h"

KeyboardHooker* KeyboardHooker::s_instance = nullptr;

Qt::KeyboardModifiers KeyboardHooker::toQtModifiers(const ModifierState& ms) {
    Qt::KeyboardModifiers m;
    if (ms.ctrl)  m |= Qt::ControlModifier;
    if (ms.shift) m |= Qt::ShiftModifier;
    if (ms.alt)   m |= Qt::AltModifier;
    if (ms.meta)  m |= Qt::MetaModifier;
    return m;
}

void KeyboardHooker::updateModifierState(ModifierState& ms, WPARAM wParam, DWORD vkCode) {
    bool down = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
    switch (vkCode) {
        case VK_CONTROL:
        case VK_LCONTROL:
        case VK_RCONTROL:  ms.ctrl  = down; break;
        case VK_SHIFT:
        case VK_LSHIFT:
        case VK_RSHIFT:    ms.shift = down; break;
        case VK_MENU:
        case VK_LMENU:
        case VK_RMENU:     ms.alt   = down; break;
        case VK_LWIN:
        case VK_RWIN:      ms.meta  = down; break;
    }
}

void KeyboardHooker::snapshotModifiersFromOS(ModifierState& ms) {
    // Check per-side virtual keys so left/right modifiers are both covered
    // (GetAsyncKeyState(VK_MENU) alone is ambiguous on some layouts).
    ms.ctrl  = ((GetAsyncKeyState(VK_LCONTROL) | GetAsyncKeyState(VK_RCONTROL)) & 0x8000) != 0;
    ms.shift = ((GetAsyncKeyState(VK_LSHIFT)   | GetAsyncKeyState(VK_RSHIFT))   & 0x8000) != 0;
    ms.alt   = ((GetAsyncKeyState(VK_LMENU)    | GetAsyncKeyState(VK_RMENU))    & 0x8000) != 0;
    ms.meta  = ((GetAsyncKeyState(VK_LWIN)     | GetAsyncKeyState(VK_RWIN))     & 0x8000) != 0;
}

LRESULT CALLBACK keyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        auto* inst = KeyboardHooker::s_instance;
        if (!inst) return CallNextHookEx(nullptr, nCode, wParam, lParam);

        auto* keyEvent = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);

        KeyboardHooker::updateModifierState(inst->m_modState, wParam, keyEvent->vkCode);

        if (inst->m_paused) {
            // When paused, only process TogglePause so the user can unpause
            if (wParam == WM_SYSKEYDOWN || wParam == WM_KEYDOWN) {
                Qt::KeyboardModifiers mods = KeyboardHooker::toQtModifiers(inst->m_modState);
                auto it = inst->m_bindings.find(HotkeyAction::TogglePause);
                if (it != inst->m_bindings.end()) {
                    for (const auto& binding : it.value()) {
                        if (binding.matchesPhysical(keyEvent->vkCode, keyEvent->scanCode,
                                                    (keyEvent->flags & LLKHF_EXTENDED) != 0, mods)) {
                            emit inst->hotkeyTriggered(it.key(), mods);
                            return 1;
                        }
                    }
                }
            }
            return CallNextHookEx(nullptr, nCode, wParam, lParam);
        }

        if (keyEvent->flags & LLKHF_INJECTED) {
            return CallNextHookEx(nullptr, nCode, wParam, lParam);
        }

        Qt::KeyboardModifiers mods = KeyboardHooker::toQtModifiers(inst->m_modState);

        // Recording pipeline: route key to recorder via PostMessage
        if (inst->m_recorderHwnd && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
            inst->m_keyRecord.vkCode = keyEvent->vkCode;
            inst->m_keyRecord.scanCode = keyEvent->scanCode;
            inst->m_keyRecord.flags = keyEvent->flags;
            inst->m_keyRecord.modifiers = mods;
            inst->m_keyRecord.ready.store(true, std::memory_order_release);
            PostMessageW(inst->m_recorderHwnd, KeyboardHooker::RecordingMessageId, 0, 0);
            return 1;
        }

        if (inst->m_recordingActive) {
            return CallNextHookEx(nullptr, nCode, wParam, lParam);
        }

        if (wParam == WM_SYSKEYDOWN || wParam == WM_KEYDOWN) {
            if (!inst) return CallNextHookEx(nullptr, nCode, wParam, lParam);

            bool overlayVisible = IsWindowVisible(inst->m_ownerHwnd);
            for (auto it = inst->m_bindings.begin();
                 it != inst->m_bindings.end(); ++it) {
                if (hotkeyActionScope(it.key()) != HotkeyScope::Global) continue;

                if (it.key() == HotkeyAction::CycleProcessWindows && overlayVisible) {
                    continue;
                }

                if (it.key() == HotkeyAction::SwitchProcessWindow && overlayVisible) {
                    continue;
                }

                for (const auto& binding : it.value()) {
                    bool match = binding.matchesPhysical(keyEvent->vkCode, keyEvent->scanCode,
                                           (keyEvent->flags & LLKHF_EXTENDED) != 0, mods);
                    if (match) {
                        // Arm modifier-release tracking synchronously, before the
                        // queued hotkey delivery, so a fast key release is never
                        // missed (the old code armed only after the show event
                        // processed, which could strand the overlay on screen).
                        if (getActionMetadata(it.key()).lifecycle == HotkeyLifecycle::OverlaySession)
                            inst->armModifierReleaseTracking(mods);
                        emit inst->hotkeyTriggered(it.key(), mods);
                        return 1;
                    }
                }
            }



            // Overlay key routing: when overlay is visible, intercept overlay-scoped
            // keys and forward via dedicated signal to prevent Windows from processing
            // them (e.g. Tab changing focus) before the widget receives them.
            //
            // Strategy: try full modifiers first (for user bindings that include Alt,
            // e.g. Alt+Right -> CycleForward). If no match and activation modifiers are
            // being tracked, strip them and retry (so default bindings like Tab -> CycleForward
            // work while Alt is held to keep the overlay open).
            if (IsWindowVisible(inst->m_ownerHwnd)) {
                auto tryMatch = [&](Qt::KeyboardModifiers tryMods) -> bool {
                    for (auto it = inst->m_bindings.begin();
                         it != inst->m_bindings.end(); ++it) {
                        if (hotkeyActionScope(it.key()) != HotkeyScope::Overlay)
                            continue;
                        for (const auto& b : it.value()) {
                            if (b.matchesPhysical(keyEvent->vkCode, keyEvent->scanCode,
                                                  (keyEvent->flags & LLKHF_EXTENDED) != 0, tryMods)) {
                                emit inst->overlayKeyTriggered(it.key(), tryMods);
                                return true;
                            }
                        }
                    }
                    return false;
                };

                if (tryMatch(mods))
                    return 1;

                if (inst->m_waitingForModifierRelease) {
                    Qt::KeyboardModifiers stripped = mods & ~inst->m_activationModifiers;
                    if (stripped != mods && tryMatch(stripped))
                        return 1;
                }
            }
        } else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
            if (keyEvent->vkCode == VK_LMENU || keyEvent->vkCode == VK_RMENU) {
                emit inst->altReleased();
            }

            if (inst->m_waitingForModifierRelease) {
                Qt::KeyboardModifiers currentMods = KeyboardHooker::toQtModifiers(inst->m_modState);
                bool allReleased = (inst->m_activationModifiers & currentMods) == 0;
                if (allReleased)
                    inst->finishModifierRelease();
            }
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

void KeyboardHooker::armModifierReleaseTracking(Qt::KeyboardModifiers mods) {
    if (mods == Qt::NoModifier)
        return;
    m_activationModifiers = mods;
    m_waitingForModifierRelease = true;
    startModifierWatchdog();
}

void KeyboardHooker::startModifierWatchdog() {
    if (!m_modWatchdog) {
        // Single lazily-created, reused timer; start() is idempotent so
        // re-arming never accumulates timers.
        m_modWatchdog = new QTimer(this);
        m_modWatchdog->setInterval(kModifierWatchdogMs);
        connect(m_modWatchdog, &QTimer::timeout, this, [this]() { checkModifierWatchdog(); });
    }
    m_modWatchdog->start();
}

void KeyboardHooker::stopModifierWatchdog() {
    if (m_modWatchdog)
        m_modWatchdog->stop();
}

void KeyboardHooker::checkModifierWatchdog() {
    if (!m_waitingForModifierRelease) {
        stopModifierWatchdog();
        return;
    }
    ModifierState ms;
    snapshotModifiersFromOS(ms);
    Qt::KeyboardModifiers pressed = toQtModifiers(ms);
    if ((m_activationModifiers & pressed) == 0)
        finishModifierRelease();
}

void KeyboardHooker::finishModifierRelease() {
    // Single exit point for "activation modifiers released". The guard plus
    // stopModifierWatchdog() ensure activationModifiersReleased() is emitted at
    // most once per armed session (keyup path and watchdog both funnel here;
    // resetActivationModifiers() never emits).
    stopModifierWatchdog();
    if (!m_waitingForModifierRelease)
        return;
    m_waitingForModifierRelease = false;
    m_activationModifiers = Qt::NoModifier;
    emit activationModifiersReleased();
}

KeyboardHooker::KeyboardHooker(HWND ownerHwnd, QObject* parent)
    : QObject(parent), m_ownerHwnd(ownerHwnd) {
    if (s_instance) {
        qCritical() << "Only one KeyboardHooker can be installed!";
        return;
    }
    m_keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, (HOOKPROC) keyboardProc, GetModuleHandle(nullptr), 0);
    if (!m_keyboardHook) {
        qCritical() << "Failed to install m_keyboardHook!";
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
    if (!m_keyboardHook) return;
    UnhookWindowsHookEx(m_keyboardHook);
    s_instance = nullptr;
}

void KeyboardHooker::setRecordingActive(bool active) {
    if (s_instance) s_instance->m_recordingActive = active;
}

void KeyboardHooker::setRecordingTarget(HWND hwnd) {
    if (!s_instance) return;
    s_instance->m_recorderHwnd = hwnd;
    s_instance->m_keyRecord.ready.store(false, std::memory_order_release);
    snapshotModifiersFromOS(s_instance->m_modState);
}

void KeyboardHooker::clearRecordingTarget() {
    if (!s_instance) return;
    s_instance->m_recorderHwnd = nullptr;
    s_instance->m_keyRecord.ready.store(false, std::memory_order_release);
}

UINT KeyboardHooker::recordingMessageId() {
    return RecordingMessageId;
}

bool KeyboardHooker::tryTakeRecordedKey(quint32& vk, quint32& scanCode,
                                         DWORD& flags, Qt::KeyboardModifiers& mods) {
    if (!s_instance) return false;
    auto& rec = s_instance->m_keyRecord;
    if (!rec.ready.load(std::memory_order_acquire))
        return false;
    vk = rec.vkCode;
    scanCode = rec.scanCode;
    flags = rec.flags;
    mods = rec.modifiers;
    rec.ready.store(false, std::memory_order_release);
    return true;
}

void KeyboardHooker::setPaused(bool paused) {
    m_paused = paused;
}

void KeyboardHooker::resetActivationModifiers() {
    stopModifierWatchdog();
    m_waitingForModifierRelease = false;
    m_activationModifiers = Qt::NoModifier;
}

void KeyboardHooker::notifyOverlayShown() {
    // Only arm if not already armed: a session armed at hotkey-match time must
    // not be clobbered by the (possibly later) show notification.
    if (m_waitingForModifierRelease)
        return;
    armModifierReleaseTracking(toQtModifiers(m_modState));
}

void KeyboardHooker::activateTrackingFromPhysicalState() {
    snapshotModifiersFromOS(m_modState);
    armModifierReleaseTracking(toQtModifiers(m_modState));
}

void KeyboardHooker::updateBindings(const HotkeyBindings& bindings) {
    m_bindings = bindings;
}
