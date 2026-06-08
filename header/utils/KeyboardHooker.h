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
    static void setRecordingActive(bool active);

signals:
    void hotkeyTriggered(HotkeyAction action, Qt::KeyboardModifiers modifiers);
    void altReleased();

private:
    HHOOK h_keyboard = nullptr;
    bool m_altDown = false;
    HWND m_ownerHwnd = nullptr;
    HotkeyBindings m_bindings;

    friend LRESULT CALLBACK keyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
};

#endif //WIN_SWITCHER_KEYBOARDHOOKER_H
