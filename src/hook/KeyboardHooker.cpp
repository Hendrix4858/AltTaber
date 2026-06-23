#include "hook/KeyboardHooker.h"
#include "lifecycle/Logger.h"
#include <QDebug>
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
    ms.ctrl  = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    ms.shift = (GetAsyncKeyState(VK_SHIFT)   & 0x8000) != 0;
    ms.alt   = (GetAsyncKeyState(VK_MENU)    & 0x8000) != 0;
    ms.meta  = ((GetAsyncKeyState(VK_LWIN) | GetAsyncKeyState(VK_RWIN)) & 0x8000) != 0;
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
                            qInfo() << "[KeyHook] paused emit TogglePause";
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

            QString keyName = HotkeyStrings::vkCodeToString(keyEvent->vkCode);

            int checkedCount = 0;
            bool overlayVisible = IsWindowVisible(inst->m_ownerHwnd);
            for (auto it = inst->m_bindings.begin();
                 it != inst->m_bindings.end(); ++it) {
                if (hotkeyActionScope(it.key()) != HotkeyScope::Global) continue;

                if (it.key() == HotkeyAction::CycleProcessWindows && overlayVisible) {
                    LOG_TRACE("[KeyHook] skip CycleProcessWindows (overlay visible)");
                    continue;
                }

                if (it.key() == HotkeyAction::SwitchProcessWindow && overlayVisible) {
                    LOG_TRACE("[KeyHook] skip SwitchProcessWindow (overlay visible)");
                    continue;
                }

                for (const auto& binding : it.value()) {
                    ++checkedCount;
                    LOG_TRACE(QString("[KeyHook]   binding: mode=%1 vk=%2 sc=%3 ext=%4 mods=%5")
                                 .arg(binding.mode == HotkeyBinding::KeyMode::Physical ? "Physical" : "Logical")
                                 .arg(binding.vkCode).arg(binding.scanCode)
                                 .arg(binding.extended).arg(binding.modifiers));
                    bool match = binding.matchesPhysical(keyEvent->vkCode, keyEvent->scanCode,
                                          (keyEvent->flags & LLKHF_EXTENDED) != 0, mods);
                    LOG_TRACE("[KeyHook]   match=" + QString::number(match));
                    if (!match)
                        LOG_TRACE("[KeyHook]   mods=" + QString::number(mods));
                    if (match) {
                        qInfo() << "[KeyHook] emit" << hotkeyActionName(it.key());
                        emit inst->hotkeyTriggered(it.key(), mods);
                        return 1;
                    }
                }
            }
            if (checkedCount == 0)
                LOG_TRACE("[KeyHook] No global actions have bindings registered");
            else
                LOG_TRACE("[KeyHook] ⚠️ No global binding matched for " + keyName);

            // Overlay key routing: when overlay is visible, intercept overlay-scoped
            // keys and forward via dedicated signal to prevent Windows from processing
            // them (e.g. Tab changing focus) before the widget receives them.
            //
            // Strategy: try full modifiers first (for user bindings that include Alt,
            // e.g. Alt+Right→CycleForward). If no match and activation modifiers are
            // being tracked, strip them and retry (so default bindings like Tab→CycleForward
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
                                qInfo() << "[KeyHook] overlay-key-routing ->"
                                        << hotkeyActionName(it.key());
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
                LOG_TRACE("[KeyHook] Alt UP");
                emit inst->altReleased();
            }

            if (inst->m_waitingForModifierRelease) {
                Qt::KeyboardModifiers currentMods = KeyboardHooker::toQtModifiers(inst->m_modState);
                bool allReleased = (inst->m_activationModifiers & currentMods) == 0;
                LOG_TRACE(QString("[KeyHook] Activation check: target=%1 current=%2 allReleased=%3")
                             .arg(inst->m_activationModifiers)
                             .arg(currentMods)
                             .arg(allReleased));
                if (allReleased) {
                    qInfo() << "[KeyHook] All activation modifiers released";
                    emit inst->activationModifiersReleased();
                    inst->m_waitingForModifierRelease = false;
                    inst->m_activationModifiers = Qt::NoModifier;
                }
            }
        }
    }
    if (nCode == HC_ACTION) {
        auto* keyEvent = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        // if (wParam == WM_SYSKEYDOWN || wParam == WM_KEYDOWN)
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
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
    LOG_TRACE("KeyboardHooker uninstalled");
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
    m_waitingForModifierRelease = false;
    m_activationModifiers = Qt::NoModifier;
    LOG_TRACE("[KeyHook] Activation modifiers reset");
}

void KeyboardHooker::notifyOverlayShown() {
    Qt::KeyboardModifiers currentMods = toQtModifiers(m_modState);
    if (currentMods != Qt::NoModifier) {
        m_activationModifiers = currentMods;
        m_waitingForModifierRelease = true;
        qInfo() << "[KeyHook] Overlay shown, tracking modifiers:" << currentMods;
    } else {
        LOG_TRACE("[KeyHook] Overlay shown but no modifiers held, nothing to track");
    }
}

void KeyboardHooker::activateTrackingFromPhysicalState() {
    snapshotModifiersFromOS(m_modState);
    Qt::KeyboardModifiers currentMods = toQtModifiers(m_modState);
    if (currentMods != Qt::NoModifier) {
        m_activationModifiers = currentMods;
        m_waitingForModifierRelease = true;
        qInfo() << "[KeyHook] Physical tracking activated, modifiers:" << currentMods;
    } else {
        LOG_TRACE("[KeyHook] Physical tracking: no modifiers held");
    }
}

void KeyboardHooker::updateBindings(const HotkeyBindings& bindings) {
    m_bindings = bindings;
    LOG_TRACE(QString("[KeyHook] Bindings updated, %1 actions registered").arg(m_bindings.size()));
    for (auto it = m_bindings.begin(); it != m_bindings.end(); ++it) {
        QStringList strs;
        for (const auto& b : it.value())
            strs << b.toString();
        LOG_TRACE(QString("[KeyHook]   %1: %2 binding(s) - %3")
                     .arg(hotkeyActionName(it.key()))
                     .arg(it.value().size())
                     .arg(strs.join(", ")));
    }
}
