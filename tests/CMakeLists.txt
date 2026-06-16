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

enable_testing()
add_test(NAME test_filter COMMAND test_filter)
add_test(NAME test_hotkey COMMAND test_hotkey)
