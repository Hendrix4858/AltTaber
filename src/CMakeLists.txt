add_subdirectory(core)
add_subdirectory(lifecycle)
add_subdirectory(utils)
add_subdirectory(hook)

target_sources(Win_Switcher PRIVATE
    # ── Core / UI ──
    widget.cpp                     widget.h                     ../ui/widget.ui
    WindowManager.cpp              WindowManager.h
    WindowGroupModel.cpp           WindowGroupModel.h
    WindowEnumerator.cpp           WindowEnumerator.h
    WindowGrouper.cpp              WindowGrouper.h
    WindowFilter.cpp               WindowFilter.h
    WindowDescriptorBuilder.cpp    WindowDescriptorBuilder.h
    ActivationHistory.cpp          ActivationHistory.h
    WindowCycleOrder.cpp           WindowCycleOrder.h
    TaskbarMouseHelper.cpp         TaskbarMouseHelper.h
    OverlayController.cpp          OverlayController.h
    SelectionController.cpp        SelectionController.h
    GroupWindowCycler.cpp          GroupWindowCycler.h
    TaskbarWindowCycler.cpp        TaskbarWindowCycler.h
    ActionRouter.cpp               ActionRouter.h
                                   WindowTypes.h
                                   OverlayView.h

    # ── Config / Hotkey ──
    HotkeyConflictResolver.cpp     HotkeyConflictResolver.h

    # ── Dialogs ──
    SettingsDialog.cpp             SettingsDialog.h             ../ui/SettingsDialog.ui
    SettingsStyleHelper.cpp        SettingsStyleHelper.h
    HotkeyPageManager.cpp          HotkeyPageManager.h
    BlockedWindowManager.cpp       BlockedWindowManager.h
    AddBlockedDialog.cpp           AddBlockedDialog.h
    UpdateDialog.cpp               UpdateDialog.h               ../ui/UpdateDialog.ui
)
