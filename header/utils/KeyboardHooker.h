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
};

#endif //WIN_SWITCHER_KEYBOARDHOOKER_H
