#ifndef WIN_SWITCHER_WINDOWMANAGER_H
#define WIN_SWITCHER_WINDOWMANAGER_H

#include <QObject>
#include "WindowTypes.h"
#include "ActivationHistory.h"
#include "WindowFilter.h"

class ConfigManager;

class WindowManager : public QObject {
    Q_OBJECT

public:
    explicit WindowManager(ConfigManager* config, HWND selfHwnd = nullptr, QObject* parent = nullptr);
    void setSelfHwnd(HWND hwnd);

    QList<WindowGroup> prepareWindowGroupList();

    void recordWindowActivation(HWND hwnd);
    ActivationHistory& activationHistory() { return m_activationHistory; }

    void reloadFilterRules();
    QList<HWND> filteredHwndsForExe(const QString& exePath);

private:
    ConfigManager* m_config = nullptr;
    HWND m_selfHwnd = nullptr;
    ActivationHistory m_activationHistory;
    WindowFilter m_filter;
};

#endif //WIN_SWITCHER_WINDOWMANAGER_H
