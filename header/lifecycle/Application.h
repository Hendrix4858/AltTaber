#ifndef WIN_SWITCHER_APPLICATION_H
#define WIN_SWITCHER_APPLICATION_H

#include <QAbstractNativeEventFilter>
#include <QApplication>
#include <QTimer>
#include "core/HotkeyAction.h"

class WindowManager;
class Widget;
class KeyboardHooker;
class TaskbarWheelHooker;
class SingleApp;
class ComInitializer;

class SessionMonitor final : public QAbstractNativeEventFilter {
    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr*) override;
};

class Application {
public:
    Application(int argc, char* argv[]);
    ~Application();

    int run();

private:
    void initConfig(const HotkeyBindings& bindings);
    void initControllers();
    void initUI();
    void initHooks(const HotkeyBindings& bindings);
    void wireSignals(const HotkeyBindings& bindings);
    static bool tryRollback();

    QApplication m_app;
    WindowManager* m_windowManager = nullptr;
    Widget* m_widget = nullptr;
    KeyboardHooker* m_keyboardHooker = nullptr;
    TaskbarWheelHooker* m_taskbarHooker = nullptr;
    SessionMonitor m_sessionMon;
    SingleApp* m_singleApp = nullptr;
    ComInitializer* m_com = nullptr;
};

#endif //WIN_SWITCHER_APPLICATION_H
