target_sources(Win_Switcher PRIVATE
    Application.cpp                 Application.h
    HotkeyService.cpp               HotkeyService.h
    IconOnlyDelegate.cpp            IconOnlyDelegate.h
    IconUtil.cpp                    IconUtil.h
    IpcServer.cpp                   IpcServer.h
    Logger.cpp                      Logger.h
    QtWin.cpp                       QtWin.h
    ScheduledTask.cpp               ScheduledTask.h
    SystemTray.cpp                  SystemTray.h
    UpdateService.cpp               UpdateService.h
                                    ComInitializer.h
                                    SingleApp.h
                                    Startup.h
)
