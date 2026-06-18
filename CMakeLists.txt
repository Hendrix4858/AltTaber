cmake_minimum_required(VERSION 3.29)
project(
    Win_Switcher
    VERSION 0.6.13
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
# --- Auto-detect Qt path based on target architecture ---
# ARM64: use QT_DIR_ARM64 env var, or derive from QT_DIR by replacing msvc2022_64 → msvc2022_arm64
# x64:   use QT_DIR env var (or fall back to default CMake search)
if(CMAKE_VS_PLATFORM_NAME STREQUAL "arm64" OR CMAKE_VS_PLATFORM_NAME STREQUAL "ARM64")
    if(DEFINED ENV{QT_DIR_ARM64} AND NOT "$ENV{QT_DIR_ARM64}" STREQUAL "")
        set(QT_BASE_DIR "$ENV{QT_DIR_ARM64}")
    elseif(DEFINED ENV{QT_DIR} AND NOT "$ENV{QT_DIR}" STREQUAL "")
        string(REPLACE "msvc2022_64" "msvc2022_arm64" QT_BASE_DIR "$ENV{QT_DIR}")
    else()
        # Fallback: hardcoded path derived from known x64 install location
        set(QT_BASE_DIR "E:/CodePath/Compiler/QT/6.8.2/msvc2022_arm64")
    endif()
    set(CMAKE_PREFIX_PATH "${QT_BASE_DIR}/lib/cmake")
    set(Qt6_DIR "${QT_BASE_DIR}/lib/cmake/Qt6")
    set(Qt6Core_DIR "${QT_BASE_DIR}/lib/cmake/Qt6Core")
    set(Qt6Gui_DIR "${QT_BASE_DIR}/lib/cmake/Qt6Gui")
    set(Qt6Widgets_DIR "${QT_BASE_DIR}/lib/cmake/Qt6Widgets")
    set(Qt6Xml_DIR "${QT_BASE_DIR}/lib/cmake/Qt6Xml")
    set(Qt6Network_DIR "${QT_BASE_DIR}/lib/cmake/Qt6Network")
else()
    if(DEFINED ENV{QT_DIR} AND NOT "$ENV{QT_DIR}" STREQUAL "")
        set(QT_BASE_DIR "$ENV{QT_DIR}")
        set(CMAKE_PREFIX_PATH "${QT_BASE_DIR}/lib/cmake")
    endif()
endif()
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
# 生成 Inno Setup 脚本（注入 @PROJECT_VERSION@）
configure_file(
        installer/setup.iss.in
        ${CMAKE_CURRENT_BINARY_DIR}/setup.iss
        @ONLY
)

include_directories(header)

find_package(Qt6 COMPONENTS
        Core
        Gui
        Widgets
        Xml
        Network
        LinguistTools
        REQUIRED)

add_executable(Win_Switcher)

add_subdirectory(tests)

# ── Infrastructure ──
target_sources(Win_Switcher PRIVATE
    src/main.cpp
    res.qrc
    ${VERSIONINFO_RC}
)

# ── Core / UI ──
target_sources(Win_Switcher PRIVATE
    src/widget.cpp                 header/widget.h                 ui/widget.ui
    src/WindowManager.cpp          header/WindowManager.h
    src/WindowGroupModel.cpp       header/WindowGroupModel.h
                                   header/WindowTypes.h
    src/WindowEnumerator.cpp       header/WindowEnumerator.h
    src/WindowGrouper.cpp          header/WindowGrouper.h
    src/WindowFilter.cpp           header/WindowFilter.h
    src/WindowDescriptorBuilder.cpp header/WindowDescriptorBuilder.h
    src/ActivationHistory.cpp      header/ActivationHistory.h
    src/WindowCycleOrder.cpp       header/WindowCycleOrder.h
    src/TaskbarMouseHelper.cpp     header/TaskbarMouseHelper.h
    src/OverlayController.cpp      header/OverlayController.h
    src/SelectionController.cpp    header/SelectionController.h
    src/GroupWindowCycler.cpp      header/GroupWindowCycler.h
    src/TaskbarWindowCycler.cpp    header/TaskbarWindowCycler.h
)

# ── Hooks ──
target_sources(Win_Switcher PRIVATE
    src/hook/KeyboardHooker.cpp    header/hook/KeyboardHooker.h
    src/hook/TaskbarWheelHooker.cpp header/hook/TaskbarWheelHooker.h
    src/hook/WheelEventProcessor.cpp header/hook/WheelEventProcessor.h
    src/hook/winEventHook.cpp      header/hook/winEventHook.h
    src/hook/uiautomation.cpp      header/hook/uiautomation.h
    src/hook/HotkeyRecorder.cpp    header/hook/HotkeyRecorder.h
)

# ── Config / Hotkey ──
target_sources(Win_Switcher PRIVATE
                                   header/core/IConfigStore.h
    src/core/ConfigManagerBase.cpp header/core/ConfigManagerBase.h
                                   header/core/ConfigManager.h
    src/core/HotkeyAction.cpp      header/core/HotkeyAction.h
                                   header/core/HotkeyActionTypes.h
                                   header/core/HotkeyStrings.h
                                   header/core/HotkeyBinding.h
    src/core/ThemeManager.cpp      header/core/ThemeManager.h
    src/HotkeyConflictResolver.cpp header/HotkeyConflictResolver.h
)

# ── Dialogs ──
target_sources(Win_Switcher PRIVATE
    src/SettingsDialog.cpp         header/SettingsDialog.h
    src/SettingsStyleHelper.cpp    header/SettingsStyleHelper.h
    src/HotkeyPageManager.cpp      header/HotkeyPageManager.h
    src/BlockedWindowManager.cpp   header/BlockedWindowManager.h
    src/AddBlockedDialog.cpp       header/AddBlockedDialog.h
                                   ui/SettingsDialog.ui
    src/UpdateDialog.cpp           header/UpdateDialog.h
                                   ui/UpdateDialog.ui
)

# ── i18n ──
target_sources(Win_Switcher PRIVATE
    src/core/LanguageManager.cpp   header/core/LanguageManager.h
)

# Translation toolchain: .ts → .qm (embedded via qt_add_resources)
qt_add_lupdate(Win_Switcher TS_FILES translations/zh_CN.ts)
qt_add_lrelease(Win_Switcher TS_FILES translations/zh_CN.ts QM_FILES_OUTPUT_VARIABLE qm_files)
set_source_files_properties(${qm_files} PROPERTIES QT_RESOURCE_ALIAS "zh_CN.qm")
qt_add_resources(Win_Switcher "translations" PREFIX "/translations" FILES ${qm_files})

# ── Lifecycle ──
target_sources(Win_Switcher PRIVATE
    src/lifecycle/Application.cpp  header/lifecycle/Application.h
    src/lifecycle/UpdateService.cpp header/lifecycle/UpdateService.h
    src/lifecycle/HotkeyService.cpp header/lifecycle/HotkeyService.h
    src/lifecycle/IconUtil.cpp     header/lifecycle/IconUtil.h
    src/lifecycle/IconOnlyDelegate.cpp header/lifecycle/IconOnlyDelegate.h
    src/lifecycle/QtWin.cpp        header/lifecycle/QtWin.h
    src/lifecycle/SystemTray.cpp   header/lifecycle/SystemTray.h
    src/lifecycle/ScheduledTask.cpp header/lifecycle/ScheduledTask.h
    src/lifecycle/Logger.cpp       header/lifecycle/Logger.h
                                   header/lifecycle/ComInitializer.h
                                   header/lifecycle/SingleApp.h
                                   header/lifecycle/Startup.h
)

# ── Utils ──
target_sources(Win_Switcher PRIVATE
    src/utils/WindowUtil.cpp       header/utils/WindowUtil.h
    src/utils/ProcessUtil.cpp       header/utils/ProcessUtil.h
    src/utils/MiscUtil.cpp         header/utils/MiscUtil.h
    src/utils/PwaDetector.cpp      header/utils/PwaDetector.h
    header/utils/Util.h
    src/utils/setWindowBlur.cpp    header/utils/setWindowBlur.h
    src/utils/UwpHelper.cpp        header/utils/UwpHelper.h
    src/utils/StartMenuHelper.cpp  header/utils/StartMenuHelper.h
    src/utils/AppExeResolver.cpp   header/utils/AppUtil.h
    src/utils/PathUtils.cpp        header/utils/PathUtils.h
)
target_link_libraries(Win_Switcher
        Qt::Core
        Qt::Gui
        Qt::Widgets
        Qt::Xml
        Qt::Network
)

target_link_libraries(${PROJECT_NAME}
        Dwmapi
        Propsys
        windowsapp
)

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "AltTaber") # 设置输出文件名
if (CMAKE_BUILD_TYPE MATCHES "Release")
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE true) # 设置为窗口应用程序, 无控制台，同时会导致Clion无法捕获输出
endif ()

# --- Architecture suffix for package naming ---

if(CMAKE_VS_PLATFORM_NAME STREQUAL "arm64" OR CMAKE_VS_PLATFORM_NAME STREQUAL "ARM64")
    set(ARCH_SUFFIX "arm64")
else()
    set(ARCH_SUFFIX "win64")
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE
        QT_DEPRECATED_WARNINGS
        UNICODE # for Windows API，使用W版本
        _UNICODE # for C/C++ 标准库
        APP_VERSION="${PROJECT_VERSION}"
        APP_ARCH="${ARCH_SUFFIX}"   # "arm64" or "win64"
)

if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE "/utf-8")
endif ()

# --- Deploy & Installer targets ---

find_program(WINDEPLOYQT windeployqt
    PATHS "${QT_BASE_DIR}/bin"
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

# --- Portable zip ---

set(ZIP_DEPENDS Win_Switcher)
if (WINDEPLOYQT)
    list(APPEND ZIP_DEPENDS deploy)
endif ()

add_custom_target(zip
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/build/output"
    COMMAND ${CMAKE_COMMAND} -E rm -f "${CMAKE_SOURCE_DIR}/build/output/AltTaber-${PROJECT_VERSION}-${ARCH_SUFFIX}.zip"
    COMMAND ${CMAKE_COMMAND} -E tar cf "${CMAKE_SOURCE_DIR}/build/output/AltTaber-${PROJECT_VERSION}-${ARCH_SUFFIX}.zip" --format=zip .
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:Win_Switcher>"
    COMMENT "Packaging portable zip (${ARCH_SUFFIX}) ..."
    DEPENDS ${ZIP_DEPENDS}
)

# --- Inno Setup installer ---

if (ISCC)
    set(installer_deps Win_Switcher)
    if (WINDEPLOYQT)
        list(APPEND installer_deps deploy)
    endif ()
    add_custom_target(installer
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/build/output"
        COMMAND ${ISCC} "/DMyAppArch=${ARCH_SUFFIX}" "/DMyAppBuildDir=${CMAKE_BINARY_DIR}" "/DMyAppSourceDir=${CMAKE_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/setup.iss"
        COMMENT "Compiling AltTaber installer (${ARCH_SUFFIX}) ..."
        DEPENDS ${installer_deps}
    )
endif ()