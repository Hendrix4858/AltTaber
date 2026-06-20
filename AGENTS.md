# AGENTS.md — AltTaber (Win_Switcher)

Qt 6 C++20 Windows Alt+Tab switcher. Single executable, tests in `tests/`, CI via GitHub Actions.

## Build & Run

```bash
# Debug (single arch, host toolchain)
cmake --preset "release-x64" # or omit --preset, use -B build/ai
cmake --build build/ai
./build/ai/Debug/AltTaber.exe
```

- **Requires**: CMake ≥3.29, MSVC 2022, Qt 6 (Core, Gui, Widgets, Xml, Network)
- **MSVC flag**: `/utf-8` is forced on MSVC (`CMakeLists.txt:211`)
- **Debug**: keep console for qDebug output; **Release**: `WIN32_EXECUTABLE=ON` → no console
- **Links**: `Dwmapi.lib`, `Propsys.lib`, `windowsapp.lib`

### Release (multi-arch portable zip + installer)

```bash
# Prerequisites: set QT_DIR_X64 and QT_DIR_ARM64 env vars
build-all.cmd                           # build both x64 + arm64
build-all.cmd x64                       # single arch
```

Or manually per arch:

```bash
cmake --preset "release-x64"                    # configure
cmake --build "build/ai/x64" --target zip          # build → windeployqt → portable zip
cmake --build "build/ai/x64" --target installer    # build → windeployqt → Inno Setup
```

| Target | Output |
|--------|--------|
| `zip` | `build/ai/output/AltTaber-{version}-{arch}.zip` (portable) |
| `installer` | `build/ai/output/AltTaber-{version}-{arch}-Setup.exe` |
| `deploy` | Runs `windeployqt` (invoked automatically by zip/installer) |

### Presets

| Preset | Arch | Env var |
|--------|------|---------|
| `release-x64` | x86-64 | `QT_DIR_X64` |
| `release-arm64` | ARM64 | `QT_DIR_ARM64` |

- **IDE hint**: `Desktop_Qt_6_8_2_MSVC2022_64bit-Debug`, build dir `build/ai/Desktop_...`

## Source Layout

| Path | Purpose |
|------|---------|
| `header/` | Root-level `.h` files (Widget, WindowManager, core domain models) |
| `header/utils/` | Pure stateless utility functions (MiscUtil, ProcessUtil, WindowUtil, AppUtil) |
| `header/hook/` | System hooks and input processing (KeyboardHooker, TaskbarWheelHooker, winEventHook, WheelEventProcessor) |
| `header/core/` | Core domain model and configuration (HotkeyAction types, ConfigManager, ThemeManager, i18n) |
| `header/lifecycle/` | Application bootstrap and platform bindings (Application, SystemTray, SingleApp, Logger, IconUtil) |
| `src/` | Matching `.cpp` files mirroring the header structure |
| `ui/` | Qt Designer `.ui` files |
| `translations/` | Qt Linguist `.ts`/`.qm` translation files (`zh_CN.ts` → `zh_CN.qm` embedded via `qt_add_resources`) |
| `installer/` | Inno Setup template (`setup.iss.in` — version injected from CMake via `configure_file`) and legacy build script (`build-installer.ps1`) |

**Includes**: `#include "core/ConfigManager.h"` not `#include "utils/ConfigManager.h"` (use the subdirectory prefix).

## Architecture (Entrypoint)

`src/main.cpp` wires everything:
1. Logger init → SingleApp guard → COM init → SystemTray → Language
2. Admin re-elevation check (`cfg().getAlwaysRunAsAdmin()`)
3. Theme → `WindowManager` → `Widget` (the overlay)
4. `KeyboardHooker` (low-level WH_KEYBOARD_LL) + `TaskbarWheelHooker` (mouse hook)
5. WinEvent hook (`EVENT_SYSTEM_FOREGROUND`) for foreground tracking
6. `Widget::warmupCache` deferred by `QTimer::singleShot(0, ...)`

## Key Classes

- **`Widget`** — Overlay QWidget. State machine: Hidden→Showing→Visible→Hiding. Handles key events, wheel, paint with acrylic blur.
- **`WindowManager`** — Builds window group list, MRU activation tracking, group window rotation.
- **`WindowGroupModel`** — `QAbstractListModel` feeding a `QListView`.
- **`OverlayController`** — Coordinates overlay show/hide state transitions and input routing.
- **`SelectionController`** — Handles visual selection highlight and directional navigation.
- **`GroupWindowCycler`** — Rotates windows within a selected app group.
- **`TaskbarWindowCycler`** — Rotates windows per app when hovering taskbar and using mouse wheel.
- **`KeyboardHooker`** — WH_KEYBOARD_LL, emits `hotkeyTriggered(action, modifiers)` and `altReleased()`. Uses `Qt::QueuedConnection`.
- **`TaskbarWheelHooker`** — WH_MOUSE_LL hook for taskbar wheel rotation.
- **`HotkeyRecorder`** — Captures and validates user-defined hotkey combos in settings.
- **`HotkeyConflictResolver`** — Detects and resolves binding conflicts.
- **`ConfigManager`** — Singleton via `cfg()`. Config file: **`config.json`** (JSON, not `config.ini` as the README claims). Path: `%APPDATA%\MrBeanCpp\AltTaber\config.json`.
- **`SystemTray`** — Singleton via `sysTray()`. Menu: Update, Settings, Pause, Restart as Admin, Startup, Display Monitor, Quit.
- **`ThemeManager`** — Dark/Light/System. Reads Windows registry for System theme detection.
- **`Application`** — QApplication subclass; owns all services, handles session management and WinEvent filtering.
- **`HotkeyService`** — Manages dynamic hotkey re-injection after config changes.
- **`UpdateService`** — Checks GitHub releases, downloads and installs updates.

## Hotkey System

Configurable JSON under `[Hotkeys]` key. Defaults hardcoded in `main.cpp:64-83`:

| Action | Default Binding |
|--------|----------------|
| ShowSwitcher | `Alt+Tab` |
| EnterGroupMode | `Alt+Grave` |
| CycleForward | `Tab` |
| CycleBackward | `Shift+Tab` |
| MoveSelectionUp | `Up` |
| MoveSelectionDown | `Down` |
| ActivateSelected | `Enter` |
| DismissSwitcher | `Escape` |
| TogglePause | (none) |

**Overlay bindings** (active only when overlay is visible): CycleForward/Backward, MoveSelectionUp/Down, ActivateSelected, DismissSwitcher, EnterGroupMode — stored separately in `Widget::m_overlayBindings`.

## Config

- File: `<userappdata>/MrBeanCpp/AltTaber/config.json` (auto-created on first run)
- Migrates from old `<appdir>/config.json` on first run after upgrade
- Settings opened with `editConfigFile()` which launches notepad; closing notepad triggers `configEdited` signal → hotkey re-injection
- Icon cache: `<appdir>/icon_cache/` by default

## Translations

- Standard Qt Linguist toolchain: `.cpp`/`.ui` → `lupdate` → `.ts` → `lrelease` → `.qm`
- `zh_CN.ts` in `translations/` is the source translation file; `.qm` is compiled at build time and embedded via `qt_add_resources` under `:/translations/`
- `LanguageManager` handles `initLanguage()` and `switchLanguage(langCode)` using standard `QTranslator`
- English is the default (source strings in code); Chinese (`zh_CN`) is the only translation pack
- Add a new language: create a new `.ts` file, add it to `qt_add_lupdate`/`qt_add_lrelease` in `CMakeLists.txt`, translate, rebuild
- Run `lupdate` manually: `cmake --build build/ai --target Win_Switcher_lupdate`
- `lrelease` runs automatically on every build via CMake pre-build command

### Workflow: Adding new `tr()` strings (e.g. new tray menu item)

1. Edit C++ code to add `tr("NewString")` and connect signals
2. Run `lupdate` **before** editing the `.ts` file:
   ```
   cmake --build build/ai --target Win_Switcher_lupdate
   ```
   This auto-generates `<message>` entries with correct `<location>` line numbers
   (uses `-locations relative` in `CMakeLists.txt:141` for stable diffs across machines)
3. Open `translations/zh_CN.ts`, find the new `<source>NewString</source>`, fill in `<translation>` with the Chinese translation
4. Rebuild — `lrelease` runs automatically as a pre-build step

## Style & Conventions

- `Q_OBJECT` in every QObject subclass (MOC enabled via `CMAKE_AUTOMOC`)
- Icons lazy-loaded per group with generation counter to discard stale async callbacks (`widget.cpp:144`)
- Include path: `include_directories(header)` → use `#include "widget.h"` not `#include "header/widget.h"`
- Namespace `Util::` for utility functions; `HotkeyStrings::` for VK code conversion
- Config enums (Theme, DisplayMonitor) use plain `enum` (not `enum class`) for readability in QSettings

## Gotchas

- **Clean Logs**: `SettingsDialog::cleanLogFiles()` calls `Logger::closeLog()` before deletion and `Logger::reopenLog()` after to release the open log file handle on disk.
- **README is wrong about config file**: it says `config.ini` but the code uses `config.json` via `ConfigManagerBase`. Trust the code.
- **Config path**: `%APPDATA%\MrBeanCpp\AltTaber\config.json`. Uninstall preserves config unless user explicitly deletes it.
- **Installer**: Inno Setup `setup.iss` builds `AltTaber-vX.Y.Z-{arch}-Setup.exe`. Update flow: download setup.exe → `/VERYSILENT` → replaces zip+bat chain.
- **AppId** must remain `AltTaber.MrBeanCpp` forever for Inno upgrade detection.
- **Project target** is `Win_Switcher`; **output binary** is `AltTaber.exe`
- **Icon cache** (`IconCacheDirectory` config) needs deletion if `icon.ico` changes on disk (Windows caches aggressively); referenced in comment at `CMakeLists.txt:21`
- **No linters, no formatters** — manual testing only
- **Admin elevation**: `ShellExecuteW(..., L"runas", ...)` re-launches; auto-start uses Scheduled Task (admin) vs Registry (non-admin) via `Startup.h`
