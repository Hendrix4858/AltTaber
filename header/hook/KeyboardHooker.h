#ifndef WIN_SWITCHER_KEYBOARDHOOKER_H
#define WIN_SWITCHER_KEYBOARDHOOKER_H

#include <Windows.h>
#include <QObject>
#include <atomic>
#include "core/HotkeyAction.h"

class KeyboardHooker : public QObject {
    Q_OBJECT

public:
    explicit KeyboardHooker(HWND ownerHwnd, QObject* parent = nullptr);
    ~KeyboardHooker() override;

    void updateBindings(const HotkeyBindings& bindings);
    void setPaused(bool paused);
    static void setRecordingActive(bool active);
    static void setRecordingTarget(HWND hwnd);
    static void clearRecordingTarget();
    static UINT recordingMessageId();
    static bool tryTakeRecordedKey(quint32& vk, quint32& scanCode,
                                   DWORD& flags, Qt::KeyboardModifiers& mods);

    void resetActivationModifiers();
    void notifyOverlayShown();
    void activateTrackingFromPhysicalState();

signals:
    void hotkeyTriggered(HotkeyAction action, Qt::KeyboardModifiers modifiers);
    void overlayKeyTriggered(HotkeyAction action, Qt::KeyboardModifiers modifiers);
    void altReleased();
    void activationModifiersReleased();

private:
    struct ModifierState {
        bool ctrl = false;
        bool shift = false;
        bool alt = false;
        bool meta = false;
    };

    struct RawKeyRecord {
        quint32 vkCode = 0;
        quint32 scanCode = 0;
        DWORD flags = 0;
        Qt::KeyboardModifiers modifiers = Qt::NoModifier;
        std::atomic<bool> ready{false};
    };

    HHOOK m_keyboardHook = nullptr;
    bool m_paused = false;
    HWND m_ownerHwnd = nullptr;
    HotkeyBindings m_bindings;

    ModifierState m_modState;
    RawKeyRecord m_keyRecord;
    HWND m_recorderHwnd = nullptr;
    bool m_recordingActive = false;

    Qt::KeyboardModifiers m_activationModifiers = Qt::NoModifier;
    bool m_waitingForModifierRelease = false;

    static KeyboardHooker* s_instance;

    static Qt::KeyboardModifiers toQtModifiers(const ModifierState& ms);
    static void updateModifierState(ModifierState& ms, WPARAM wParam, DWORD vkCode);
    static void snapshotModifiersFromOS(ModifierState& ms);

    static constexpr UINT RecordingMessageId = WM_APP + 0x100;

    friend LRESULT CALLBACK keyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
};

#endif //WIN_SWITCHER_KEYBOARDHOOKER_H
