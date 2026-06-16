#ifndef WIN_SWITCHER_HOTKEYSERVICE_H
#define WIN_SWITCHER_HOTKEYSERVICE_H

#include <QObject>
#include "core/HotkeyAction.h"

class Widget;
class KeyboardHooker;
class TaskbarWheelHooker;
class ConfigManager;

class HotkeyService : public QObject {
    Q_OBJECT

public:
    explicit HotkeyService(ConfigManager* config, QObject* parent = nullptr);
    ~HotkeyService() override;

    void init(Widget* widget, const HotkeyBindings& bindings);
    void wireSignals(Widget* widget);
    void reloadFromConfig();

private:
    ConfigManager* m_config;
    KeyboardHooker* m_keyboardHooker = nullptr;
    TaskbarWheelHooker* m_taskbarHooker = nullptr;
};

#endif //WIN_SWITCHER_HOTKEYSERVICE_H
