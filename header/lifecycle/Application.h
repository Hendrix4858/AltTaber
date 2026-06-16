#ifndef WIN_SWITCHER_APPLICATION_H
#define WIN_SWITCHER_APPLICATION_H

#include <QAbstractNativeEventFilter>
#include <QApplication>
#include <QTimer>

class WindowManager;
class Widget;
class SingleApp;
class ComInitializer;
class ConfigManager;
class UpdateService;
class HotkeyService;

class SessionMonitor final : public QAbstractNativeEventFilter {
    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr*) override;
};

class Application {
public:
    Application(int argc, char* argv[]);
    ~Application();

    int run();

private:
    void initControllers();
    void initUI();
    void initHotkeys();

    ConfigManager* m_config = nullptr;
    UpdateService* m_updateService = nullptr;
    HotkeyService* m_hotkeyService = nullptr;

    QApplication m_app;
    WindowManager* m_windowManager = nullptr;
    Widget* m_widget = nullptr;
    SessionMonitor m_sessionMon;
    SingleApp* m_singleApp = nullptr;
    ComInitializer* m_com = nullptr;
};

#endif //WIN_SWITCHER_APPLICATION_H
