#ifndef WIN_SWITCHER_KEYBOARDHOOKER_H
#define WIN_SWITCHER_KEYBOARDHOOKER_H

#include <Windows.h>
#include <QObject>

class KeyboardHooker : public QObject {
    Q_OBJECT

public:
    explicit KeyboardHooker(HWND ownerHwnd, QObject* parent = nullptr);
    ~KeyboardHooker() override;

signals:
    void requestShow();
    void altTabPressed(Qt::KeyboardModifiers modifiers);
    void altGravePressed(Qt::KeyboardModifiers modifiers);
    void altReleased();

private:
    HHOOK h_keyboard = nullptr;
    bool m_altDown = false;
    HWND m_ownerHwnd = nullptr;

    friend LRESULT CALLBACK keyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
};

#endif //WIN_SWITCHER_KEYBOARDHOOKER_H
