#ifndef WIN_SWITCHER_WINDOWMANAGER_H
#define WIN_SWITCHER_WINDOWMANAGER_H

#include <QObject>
#include "WindowTypes.h"
#include "ActivationHistory.h"

class WindowManager : public QObject {
    Q_OBJECT

public:
    explicit WindowManager(HWND selfHwnd = nullptr, QObject* parent = nullptr);
    void setSelfHwnd(HWND hwnd);

    QList<WindowGroup> prepareWindowGroupList();

    void recordWindowActivation(HWND hwnd);
    ActivationHistory& activationHistory() { return m_activationHistory; }

private:
    HWND m_selfHwnd = nullptr;
    ActivationHistory m_activationHistory;
};

#endif //WIN_SWITCHER_WINDOWMANAGER_H
