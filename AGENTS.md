# AGENTS.md — AltTaber (Win_Switcher)

Qt 6 C++20 Windows Alt+Tab switcher. Single executable, tests in `tests/`, CI via GitHub Actions.

## Build & Run

```bash
# Debug (host toolchain, Debug config) — use build/ai for AI builds
cmake -B build/ai
cmake --build build/ai
./build/ai/Debug/AltTaber.exe

# Release via preset (paths per CMakePresets.json)
cmake --preset "release-x64"
cmake --build "build/x64" --config Release
./build/x64/Release/AltTaber.exe

# Release directly (no preset)
cmake -B build/ai -DCMAKE_BUILD_TYPE=Release
cmake --build build/ai --config Release
./build/ai/Release/AltTaber.exe
```

- **Requires**: CMake ≥3.29, MSVC 2022, Qt 6 (Core, Gui, Widgets, Xml, Network, LinguistTools)
- **MSVC flag**: `/utf-8` forced on MSVC (`CMakeLists.txt:218`)
- **Debug**: console for qDebug; **Release**: `WIN32_EXECUTABLE=ON` → no console
- **Links**: `Dwmapi.lib`, `Propsys.lib`, `windowsapp.lib`, `shlwapi.lib`

### Release packaging

| Target | Output |
|--------|--------|
| `zip` | `build/output/AltTaber-{version}-{arch}.zip` (portable) |
| `installer` | `build/output/AltTaber-{version}-{arch}-Setup.exe` (Inno Setup) |

```bash
# Using preset
cmake --preset "release-x64"
cmake --build "build/x64" --target zip
cmake --build "build/x64" --target installer

# Using build/ai (AI builds)
cmake -B build/ai -DCMAKE_BUILD_TYPE=Release
cmake --build build/ai --target zip
cmake --build build/ai --target installer
```

`windeployqt` runs automatically as a dependency of zip/installer (`deploy` target). Inno Setup path hardcoded in `CMakeLists.txt:232-240`.

### Presets

| Preset | Arch | Build dir | Env var |
|--------|------|-----------|---------|
| `release-x64` | x86-64 | `build/x64` | `QT_DIR_X64` |
| `release-arm64` | ARM64 | `build/arm64` | `QT_DIR_ARM64` |

### Tests

```bash
cmake --preset "release-x64"
cmake --build "build/x64"
cd build/x64
ctest -C Release --output-on-failure
```

Three test executables: `test_filter` (WindowFilter), `test_hotkey` (HotkeyConflictResolver), `test_overlay` (OverlayController state machine). Each links `Qt6::Test`. Sources in `tests/`.

## Source Layout

| Path | Purpose |
|------|---------|
| `header/` | All `.h` files, subtree mirrors `src/` |
| `header/core/` | Config, hotkey types, ThemeManager, LanguageManager |
| `header/hook/` | KeyboardHooker, TaskbarWheelHooker, winEventHook, uiautomation, HotkeyRecorder |
| `header/lifecycle/` | Application, SystemTray, SingleApp, Logger, IconUtil, UpdateService, HotkeyService, ComInitializer, Startup, ScheduledTask |
| `header/utils/` | WindowUtil, ProcessUtil, MiscUtil, PwaDetector, UwpHelper, AppUtil, PathUtils, setWindowBlur, StartMenuHelper |
| `src/` | `.cpp` files mirroring `header/` structure |
| `ui/` | Qt Designer `.ui` files |
| `translations/` | `zh_CN.ts` → `zh_CN.qm` (embedded via `qt_add_resources`) |
| `tests/` | CTest-based unit tests (3 suites) |
| `installer/` | `setup.iss.in` (version injected via `configure_file`), `ChineseSimplified.isl` |

**Includes**: `#include "core/ConfigManager.h"` (subdirectory prefix). `include_directories(header)` set in root CMakeLists.txt.

## Architecture

Entrypoint: `src/main.cpp` → `Application` (`src/lifecycle/Application.cpp`). Bootstrap order:
1. Logger → COM init → SingleApp guard → SystemTray → Language
2. Admin re-elevation check (`cfg().getAlwaysRunAsAdmin()`)
3. Theme → WindowManager → Widget (overlay)
4. ActionRouter → HotkeyService (hooks: WH_KEYBOARD_LL + WH_MOUSE_LL)
5. WinEvent hook (`EVENT_SYSTEM_FOREGROUND`)
6. `Widget::warmupCache` deferred via `QTimer::singleShot(0, ...)`

Key singletons: `cfg()` (ConfigManager), `sysTray()` (SystemTray).

## Hotkey System

- Config key `[Hotkeys]` in JSON. Defaults in `HotkeyBinding.h:69-87`.
- Bindings use **physical scan codes** (not VK) by default — `makePhysicalBinding()`.
- Overlay-only actions (CycleForward, CycleBackward, MoveSelectionUp/Down, ActivateSelected, DismissSwitcher, ExpandGroup) stored in `Widget::m_overlayBindings`.
- `HotkeyService` re-injects hotkeys when `cfg().configEdited` fires (notepad editor closes).

## Config

- File: `%APPDATA%\MrBeanCpp\AltTaber\config.json` — auto-created, preserved on uninstall.
- `editConfigFile()` launches notepad; closing it triggers `configEdited` → hotkey reload.
- Icon cache: `<appdir>/icon_cache/` by default.

## Translations

- English = source strings; `zh_CN.ts` in `translations/` is the single translation pack.
- `.qm` compiled at build via `qt_add_lrelease` (pre-build step).
- Adding new `tr()` strings: run `lupdate` first, edit `.ts`, rebuild:
  ```bash
  cmake --build build/ai --target Win_Switcher_lupdate
  ```

## Style & Conventions

- `Q_OBJECT` in every QObject subclass (AUTOMOC enabled).
- Icons lazy-loaded per group with generation counter (`widget.cpp:144`).
- Namespace `Util::` for utility functions; `HotkeyStrings::` for VK code conversion.
- Config enums (Theme, DisplayMonitor) use plain `enum` (not `enum class`) for QSettings.
- **No linters, no formatters** — manual testing only.

## Gotchas

- **README is wrong about config file**: says `config.ini` but code uses `config.json`. Trust the code.
- **No `build-all.cmd`** — the AGENTS previously referenced a script that doesn't exist. Use presets manually.
- **Project target** `Win_Switcher`; output binary `AltTaber.exe`.
- **AppId** must stay `AltTaber.MrBeanCpp` for Inno upgrade detection.
- **Icon cache**: delete if `icon.ico` changes on disk (Windows caches aggressively).
- **Admin elevation**: `ShellExecuteW(..., L"runas", ...)` re-launches. Auto-start: Scheduled Task (admin) vs Registry (non-admin).
- **Clean Logs**: `SettingsDialog::cleanLogFiles()` calls `Logger::closeLog()` before deletion and `Logger::reopenLog()` after.
- **`uiautomation.h`** is a wrapper around Windows UIAutomation API — used by taskbar wheel detection.
- **CI**: `build.yml` runs two matrix jobs (x64, arm64). Release tags (`v*`) build zip + installer + GitHub Release.
