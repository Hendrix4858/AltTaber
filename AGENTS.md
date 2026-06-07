# AGENTS.md — AltTaber (Win_Switcher)

Qt 6 C++20 Windows Alt+Tab switcher. Single executable, no tests, no CI.

## Build & Run

```bash
# Set QT_DIR to your Qt 6 installation (e.g. C:/Qt/6.8.2/msvc2022_64)
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/Debug/AltTaber.exe
```

- **Preset hint** (from IDE): `Desktop_Qt_6_8_2_MSVC2022_64bit-Debug`, build dir `build/Desktop_...`
- **Requires**: CMake ≥3.29, MSVC 2022, Qt 6 (Core, Gui, Widgets, Xml, Network)
- **MSVC flag**: `/utf-8` is forced on MSVC (`CMakeLists.txt:130`)
- **Release**: `WIN32_EXECUTABLE=ON` → no console; Debug keeps console for qDebug output
- **Links**: `Dwmapi.lib`

## Source Layout

| Path | Purpose |
|------|---------|
| `header/` | All `.h` files (flat under `header/`, utils under `header/utils/`) |
| `src/` | Matching `.cpp` files |
| `header/utils/Util.h` | Umbrella: includes MiscUtil+ProcessUtil+IconUtil+WindowUtil |
| `header/utils/AppUtil.h` | Umbrella: includes UwpHelper+StartMenuHelper+AppExeResolver |
| `ui/` | Qt Designer `.ui` files |
| `translations/` | JSON translation files (`zh_CN.json` bundled via `res.qrc`) |

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
- **`KeyboardHooker`** — WH_KEYBOARD_LL, emits `hotkeyTriggered(action, modifiers)` and `altReleased()`. Uses `Qt::QueuedConnection`.
- **`ConfigManager`** — Singleton via `cfg()`. Config file: **`config.json`** (JSON, not `config.ini` as the README claims).
- **`SystemTray`** — Singleton via `sysTray()`. Menu: Update, Settings, Pause, Restart as Admin, Startup, Display Monitor, Quit.
- **`ThemeManager`** — Dark/Light/System. Reads Windows registry for System theme detection.

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

- File: `<appdir>/config.json` (auto-created on first run)
- Settings opened with `editConfigFile()` which launches notepad; closing notepad triggers `configEdited` signal → hotkey re-injection
- Icon cache: `<appdir>/icon_cache/` by default

## Translations

- Custom `JsonTranslator` (subclass of `QTranslator`) reads JSON translation maps
- `resources.qrc` embeds `translations/zh_CN.json` under `:/translations/`
- `LanguageManager` handles `initLanguage()` and `switchLanguage(langCode)`

## Style & Conventions

- `Q_OBJECT` in every QObject subclass (MOC enabled via `CMAKE_AUTOMOC`)
- Icons lazy-loaded per group with generation counter to discard stale async callbacks (`widget.cpp:144`)
- Include path: `include_directories(header)` → use `#include "widget.h"` not `#include "header/widget.h"`
- Namespace `Util::` for utility functions; `HotkeyStrings::` for VK code conversion
- Config enums (Theme, DisplayMonitor) use plain `enum` (not `enum class`) for readability in QSettings

## Gotchas

- **README is wrong about config file**: it says `config.ini` but the code uses `config.json` via `ConfigManagerBase`. Trust the code.
- **Project target** is `Win_Switcher`; **output binary** is `AltTaber.exe`
- **Alt release** handling: When the overlay is visible and Alt is released, the selected window is activated and the overlay hides. The `stayOpenOnAltRelease` option overrides this.
- **Icon cache** (`IconCacheDirectory` config) needs deletion if `icon.ico` changes on disk (Windows caches aggressively); referenced in comment at `CMakeLists.txt:21`
- **No tests, no CI, no linters, no formatters** — manual testing only
- **Admin elevation**: `ShellExecuteW(..., L"runas", ...)` re-launches; auto-start uses Scheduled Task (admin) vs Registry (non-admin) via `Startup.h`
