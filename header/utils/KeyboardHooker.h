#ifndef WIN_SWITCHER_KEYBOARDHOOKER_H
#define WIN_SWITCHER_KEYBOARDHOOKER_H

#include <Windows.h>
#include <QObject>
#include "utils/HotkeyAction.h"

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

signals:
    void hotkeyTriggered(HotkeyAction action, Qt::KeyboardModifiers modifiers);
    void altReleased();
    void activationModifiersReleased();

private:
    HHOOK h_keyboard = nullptr;
    bool m_paused = false;
    HWND m_ownerHwnd = nullptr;
    HotkeyBindings m_bindings;

    Qt::KeyboardModifiers m_activationModifiers = Qt::NoModifier;
    bool m_waitingForModifierRelease = false;

    friend LRESULT CALLBACK keyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
};

#endif //WIN_SWITCHER_KEYBOARDHOOKER_H
