#ifndef WIN_SWITCHER_HOTKEYSERVICE_H
#define WIN_SWITCHER_HOTKEYSERVICE_H

#include <QObject>
#include "core/HotkeyAction.h"

class Widget;
class ActionRouter;
class KeyboardHooker;
class TaskbarWheelHooker;
class ConfigManager;

class HotkeyService : public QObject {
    Q_OBJECT

public:
    explicit HotkeyService(ConfigManager* config, QObject* parent = nullptr);
    ~HotkeyService() override;

    void init(Widget* widget, ActionRouter* router, const HotkeyBindings& bindings);
    void wireSignals(Widget* widget);
    void reloadFromConfig();

    KeyboardHooker* keyboardHooker() const { return m_keyboardHooker; }

private slots:
    void retryFallbackShow();

private:
    ConfigManager* m_config;
    ActionRouter* m_router = nullptr;
    KeyboardHooker* m_keyboardHooker = nullptr;
    TaskbarWheelHooker* m_taskbarHooker = nullptr;

    Widget* m_widget = nullptr;
    QTimer* m_retryTimer = nullptr;
    int m_retryCount = 0;
    static constexpr int kMaxRetries = 5;
    static constexpr int kRetryIntervalMs = 20;
};

#endif //WIN_SWITCHER_HOTKEYSERVICE_H
