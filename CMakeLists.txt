cmake_minimum_required(VERSION 3.29)
project(
    Win_Switcher
    VERSION 0.7.12
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
# --- Auto-detect Qt path based on target architecture ---
# ARM64: use QT_DIR_ARM64 env var, or derive from QT_DIR by replacing msvc2022_64 → msvc2022_arm64
# x64:   use QT_DIR env var (or fall back to default CMake search)
if(CMAKE_VS_PLATFORM_NAME STREQUAL "arm64" OR CMAKE_VS_PLATFORM_NAME STREQUAL "ARM64"
   OR CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|AARCH64")
    if(DEFINED ENV{QT_DIR_ARM64} AND NOT "$ENV{QT_DIR_ARM64}" STREQUAL "")
        set(QT_BASE_DIR "$ENV{QT_DIR_ARM64}")
    elseif(DEFINED ENV{QT_DIR} AND NOT "$ENV{QT_DIR}" STREQUAL "")
        string(REPLACE "msvc2022_64" "msvc2022_arm64" QT_BASE_DIR "$ENV{QT_DIR}")
    elseif(DEFINED ENV{QT_ROOT_DIR} AND NOT "$ENV{QT_ROOT_DIR}" STREQUAL "")
        set(QT_BASE_DIR "$ENV{QT_ROOT_DIR}")
    else()

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
    elseif(DEFINED ENV{QT_ROOT_DIR} AND NOT "$ENV{QT_ROOT_DIR}" STREQUAL "")
        set(QT_BASE_DIR "$ENV{QT_ROOT_DIR}")
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

find_package(Qt6 COMPONENTS
        Core
        Gui
        Widgets
        Xml
        Network
        LinguistTools
        REQUIRED)

add_executable(Win_Switcher)

target_include_directories(Win_Switcher PRIVATE src)

add_executable(AltTaberCtl tools/AltTaberCtl/main.cpp)
target_link_libraries(AltTaberCtl PRIVATE Qt::Core Qt::Network)

add_subdirectory(tests)

# ── Infrastructure ──
target_sources(Win_Switcher PRIVATE
    src/main.cpp
    res.qrc
    ${VERSIONINFO_RC}
)

add_subdirectory(translations)
add_subdirectory(src)
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
        shlwapi
)

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "AltTaber") # 设置输出文件名
set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE true)

# --- Architecture suffix for package naming ---

if(CMAKE_VS_PLATFORM_NAME STREQUAL "arm64" OR CMAKE_VS_PLATFORM_NAME STREQUAL "ARM64"
   OR CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|AARCH64"
   OR "$ENV{VSCMD_ARG_TGT_ARCH}" STREQUAL "arm64")
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
    HINTS
        "$ENV{QT_HOST_PATH}/bin"
        "${QT_BASE_DIR}/bin"
    DOC "Qt deployment tool (windeployqt)"
)
message(STATUS "QT_BASE_DIR=${QT_BASE_DIR}")
message(STATUS "WINDEPLOYQT=${WINDEPLOYQT}")

if(DEFINED ENV{ISCC_DIR})
    list(APPEND CMAKE_PROGRAM_PATH "$ENV{ISCC_DIR}")
endif()
find_program(ISCC iscc
    DOC "Inno Setup Compiler (ISCC.exe)"
)

if (WINDEPLOYQT)
    add_custom_target(deploy
        COMMAND ${WINDEPLOYQT}
                "$<IF:$<CONFIG:Debug>,--debug,--release>"
                --no-translations --no-svg --no-opengl-sw --no-system-d3d-compiler --no-system-dxc-compiler --no-compiler-runtime --verbose 2
                "$<TARGET_FILE:Win_Switcher>"
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
    set(installer_deps Win_Switcher AltTaberCtl)
    if (WINDEPLOYQT)
        list(APPEND installer_deps deploy)
    endif ()
    add_custom_target(installer
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/build/output"
        COMMAND ${ISCC} "/DMyAppArch=${ARCH_SUFFIX}" "/DMyAppBuildDir=$<TARGET_FILE_DIR:Win_Switcher>" "/DMyAppSourceDir=${CMAKE_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/setup.iss"
        COMMENT "Compiling AltTaber installer (${ARCH_SUFFIX}) ..."
        DEPENDS ${installer_deps}
    )
endif ()