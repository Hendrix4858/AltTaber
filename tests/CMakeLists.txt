find_package(Qt6 COMPONENTS Core Gui Widgets Test REQUIRED)

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")
endif()

# --- WindowFilter tests ---
add_executable(test_filter
    test_filter_main.cpp
    ${CMAKE_SOURCE_DIR}/src/WindowFilter.cpp
)
target_include_directories(test_filter PRIVATE ${CMAKE_SOURCE_DIR}/header)
target_link_libraries(test_filter PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test)

# --- HotkeyConflictResolver tests ---
add_executable(test_hotkey
    test_hotkey_main.cpp
    ${CMAKE_SOURCE_DIR}/src/HotkeyConflictResolver.cpp
    ${CMAKE_SOURCE_DIR}/src/core/HotkeyAction.cpp
)
target_include_directories(test_hotkey PRIVATE ${CMAKE_SOURCE_DIR}/header)
target_link_libraries(test_hotkey PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test)

# --- OverlayController state machine tests ---
set(CMAKE_AUTOMOC ON)
add_executable(test_overlay
    test_overlay_main.cpp
    test_overlay_stubs.cpp
    ${CMAKE_SOURCE_DIR}/src/OverlayController.cpp
    ${CMAKE_SOURCE_DIR}/src/core/HotkeyAction.cpp
    # Q_OBJECT headers — AUTOMOC generates MOC metadata from these
    ${CMAKE_SOURCE_DIR}/header/WindowManager.h
    ${CMAKE_SOURCE_DIR}/header/WindowGroupModel.h
    ${CMAKE_SOURCE_DIR}/header/OverlayController.h
    ${CMAKE_SOURCE_DIR}/header/core/ConfigManagerBase.h
    ${CMAKE_SOURCE_DIR}/header/core/ThemeManager.h
    ${CMAKE_SOURCE_DIR}/header/lifecycle/SystemTray.h
)
target_include_directories(test_overlay PRIVATE ${CMAKE_SOURCE_DIR}/header)
target_link_libraries(test_overlay PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test)

enable_testing()
add_test(NAME test_filter COMMAND test_filter)
add_test(NAME test_hotkey COMMAND test_hotkey)
add_test(NAME test_overlay COMMAND test_overlay)
