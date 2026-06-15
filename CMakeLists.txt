cmake_minimum_required(VERSION 3.29)
project(
    Win_Switcher
    VERSION 0.6.0
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_PREFIX_PATH "$ENV{QT_DIR}/lib/cmake")
list(APPEND CMAKE_AUTOUIC_SEARCH_PATHS ui) # 如果.ui文件不在.cpp同一目录下，需要添加搜索路径

set(COMPANY_NAME "MrBeanCpp")
set(FILE_DESCRIPTION "AltTaber - Window Switcher")
set(PRODUCT_NAME "AltTaber")
# 设置图标的相对路径
# 图标会被缓存，修改后不生效的话，需要删除AppData\Local\IconCache.db & 重启资源管理器，该文件在PC重启后自动创建
set(ICON_PATH "${CMAKE_CURRENT_SOURCE_DIR}/img/icon.ico")

set(VERSIONINFO_RC "${CMAKE_CURRENT_BINARY_DIR}/VersionInfo.rc")
# 处理.in模板文件 生成版本信息文件
configure_file(
        VersionInfo.rc.in
        ${VERSIONINFO_RC}
        @ONLY # 只处理@var@格式的变量
)

include_directories(header)

find_package(Qt6 COMPONENTS
        Core
        Gui
        Widgets
        Xml
        Network
        REQUIRED)

add_executable(Win_Switcher src/main.cpp
        ${VERSIONINFO_RC}
        res.qrc
        src/widget.cpp
        header/widget.h
        ui/widget.ui
        src/WindowGroupModel.cpp
        header/WindowGroupModel.h
        src/WindowManager.cpp
        header/WindowManager.h
        header/WindowTypes.h
        src/WindowEnumerator.cpp
        header/WindowEnumerator.h
        src/WindowGrouper.cpp
        src/WindowFilter.cpp
        header/WindowFilter.h
        src/WindowDescriptorBuilder.cpp
        header/WindowDescriptorBuilder.h
        header/WindowGrouper.h
        src/ActivationHistory.cpp
        header/ActivationHistory.h
        src/WindowCycleOrder.cpp
        header/WindowCycleOrder.h
        src/OverlayController.cpp
        header/OverlayController.h
        src/SelectionController.cpp
        header/SelectionController.h
        src/GroupWindowCycler.cpp
        header/GroupWindowCycler.h
        src/TaskbarWindowCycler.cpp
        header/TaskbarWindowCycler.h
        src/hook/winEventHook.cpp
        header/hook/winEventHook.h
        src/utils/WindowUtil.cpp
        header/utils/WindowUtil.h
        src/utils/ProcessUtil.cpp
        header/utils/ProcessUtil.h
        src/lifecycle/IconUtil.cpp
        header/lifecycle/IconUtil.h
        src/utils/MiscUtil.cpp
        header/utils/MiscUtil.h
        header/utils/Util.h
        header/utils/setWindowBlur.h
        src/utils/setWindowBlur.cpp
        src/lifecycle/IconOnlyDelegate.cpp
        header/lifecycle/IconOnlyDelegate.h
        header/hook/uiautomation.h
        src/hook/uiautomation.cpp
        src/utils/UwpHelper.cpp
        header/utils/UwpHelper.h
        src/utils/StartMenuHelper.cpp
        header/utils/StartMenuHelper.h
        src/utils/AppExeResolver.cpp
        header/utils/AppUtil.h
        src/hook/KeyboardHooker.cpp
        header/hook/KeyboardHooker.h
        src/hook/TaskbarWheelHooker.cpp
        header/hook/TaskbarWheelHooker.h
        src/lifecycle/QtWin.cpp
        header/lifecycle/QtWin.h
        header/lifecycle/ComInitializer.h
        header/lifecycle/SingleApp.h
        header/lifecycle/Startup.h
        header/lifecycle/SystemTray.h
        src/lifecycle/SystemTray.cpp
        header/core/ConfigManager.h
        header/core/ConfigManagerBase.h
        src/core/ConfigManagerBase.cpp
        header/core/HotkeyAction.h
        header/core/HotkeyActionTypes.h
        header/core/HotkeyStrings.h
        header/core/HotkeyBinding.h
        src/core/HotkeyAction.cpp
        src/hook/HotkeyRecorder.cpp
        header/hook/HotkeyRecorder.h
        src/UpdateDialog.cpp
        header/UpdateDialog.h
        ui/UpdateDialog.ui
        src/lifecycle/ScheduledTask.cpp
        header/lifecycle/ScheduledTask.h
        header/SettingsDialog.h
        src/SettingsDialog.cpp
        header/AddBlockedDialog.h
        src/AddBlockedDialog.cpp
        ui/SettingsDialog.ui
        header/core/LanguageManager.h
        src/core/LanguageManager.cpp
        header/core/JsonTranslator.h
        src/core/JsonTranslator.cpp
        header/lifecycle/Logger.h
        src/lifecycle/Logger.cpp
        header/core/ThemeManager.h
        src/core/ThemeManager.cpp
        header/hook/WheelEventProcessor.h
        src/hook/WheelEventProcessor.cpp
        header/lifecycle/Application.h
        src/lifecycle/Application.cpp)
target_link_libraries(Win_Switcher
        Qt::Core
        Qt::Gui
        Qt::Widgets
        Qt::Xml
        Qt::Network
)

target_link_libraries(${PROJECT_NAME}
        Dwmapi
)

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "AltTaber") # 设置输出文件名
if (CMAKE_BUILD_TYPE MATCHES "Release")
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE true) # 设置为窗口应用程序, 无控制台，同时会导致Clion无法捕获输出
endif ()

target_compile_definitions(${PROJECT_NAME} PRIVATE
        QT_DEPRECATED_WARNINGS
        UNICODE # for Windows API，使用W版本
        _UNICODE # for C/C++ 标准库
        APP_VERSION="${PROJECT_VERSION}"
)

if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE "/utf-8")
endif ()

# --- Deploy & Installer targets ---

find_program(WINDEPLOYQT windeployqt
    PATHS "$ENV{QT_DIR}/bin"
    DOC "Qt deployment tool (windeployqt)"
)

find_program(ISCC iscc
    PATHS
        "E:/CodePath/Pack/Inno"
        "C:/Program Files (x86)/Inno Setup 6"
        "C:/Program Files/Inno Setup 6"
        "C:/Program Files (x86)/Inno Setup"
        "C:/Program Files/Inno Setup"
    DOC "Inno Setup Compiler (ISCC.exe)"
)

if (WINDEPLOYQT)
    add_custom_target(deploy
        COMMAND ${WINDEPLOYQT} --release --no-translations --no-opengl-sw "$<TARGET_FILE:Win_Switcher>"
        COMMENT "Deploying Qt runtime DLLs ..."
        DEPENDS Win_Switcher
    )
endif ()

if (ISCC)
    set(installer_deps Win_Switcher)
    if (WINDEPLOYQT)
        list(APPEND installer_deps deploy)
    endif ()
    add_custom_target(installer
        COMMAND ${ISCC} "/DMyAppVersion=${PROJECT_VERSION}" "${CMAKE_SOURCE_DIR}/installer/setup.iss"
        COMMENT "Compiling AltTaber installer ..."
        DEPENDS ${installer_deps}
    )
endif ()
