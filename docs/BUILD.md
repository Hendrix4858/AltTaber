# Building AltTaber

Developer guide for building, testing, and packaging AltTaber from source.

[中文](../README.md) [en](README.en.md) | [构建](BUILD_CN.md) · [翻译](TRANSLATIONS_CN.md)　|　[Build](BUILD.md) · [Translations](TRANSLATIONS.md)

---

## Prerequisites

| Requirement | Version | Notes |
|---|---|---|
| Windows | 10+ | Windows-only project |
| CMake | ≥ 3.29 | |
| MSVC | VS 2022 (17) | C++20, `/utf-8` enforced globally |
| Qt | 6.8+ | Components: `Core`, `Gui`, `Widgets`, `Xml`, `Network`, `LinguistTools` |
| Windows SDK | 10.0+ | Installed with Visual Studio |
| Inno Setup | 6.5+ | Optional — only needed for the `installer` target |
| windeployqt | ships with Qt | Optional — needed for `deploy` / `zip` / `installer` targets |

Links: `Dwmapi`, `Propsys`, `windowsapp`, `shlwapi` (all ship with the Windows SDK).

## Qt Path Configuration

Qt is located via environment variables, checked in this order:

**x64 builds:**

1. `QT_DIR_X64` → used as `QT_DIR`
2. `QT_DIR`
3. `QT_ROOT_DIR`

**ARM64 builds:**

1. `QT_DIR_ARM64`
2. `QT_DIR` with `msvc2022_64` replaced by `msvc2022_arm64`
3. `QT_ROOT_DIR`

Example (PowerShell):

```powershell
$env:QT_DIR_X64   = "D:\Dev\IDE\Qt\6.8.3\msvc2022_64"
$env:QT_DIR_ARM64 = "D:\Dev\IDE\Qt\6.8.3\msvc2022_arm64"
```

## Build Presets

Defined in `CMakePresets.json`:

| Configure preset | Generator | Output dir | Configurations |
|---|---|---|---|
| `release-x64` | Visual Studio 17 2022 | `build/x64` | Release |
| `release-arm64` | Visual Studio 17 2022 | `build/arm64` | Release |
| `ninja-x64` | Ninja Multi-Config | `build/ninja-x64` | Debug + Release |
| `ninja-arm64` | Ninja Multi-Config | `build/ninja-arm64` | Debug + Release |

Build presets: `release-x64`, `release-arm64`, `ninja-x64-debug`, `ninja-x64-release`, `ninja-arm64-debug`, `ninja-arm64-release`.

> Ninja presets require the matching **Native Tools Command Prompt** (or `vcvars64.bat` / `vcvarsamd64_arm64.bat`) so that `cl.exe` and `ninja` are on `PATH`.

## Common Commands

```shell
# Configure + build (VS generator)
cmake --preset release-x64
cmake --build build/x64 --config Release

# Configure + build (Ninja)
cmake --preset ninja-x64
cmake --build build/ninja-x64 --config Release        # or Debug

# Run unit tests
cmake --build build/ninja-x64 --config Debug --target test_filter test_hotkey test_overlay
ctest --test-dir build/ninja-x64 -C Debug

# Deploy Qt runtime DLLs next to AltTaber.exe
cmake --build build/x64 --target deploy

# Package portable zip -> build/output/AltTaber-<version>-<arch>.zip
cmake --build build/x64 --target zip

# Package installer -> build/output/AltTaber-v<version>-<arch>-Setup.exe
cmake --build build/x64 --target installer
```

The `zip` and `installer` targets automatically depend on `Win_Switcher`, `AltTaberCtl`, and `deploy` (when `windeployqt` is found), so a single target build produces a ready-to-distribute artifact.

### Installer notes

- Set `ISCC_DIR` to your Inno Setup directory (containing `ISCC.exe`) before configuring, e.g. `$env:ISCC_DIR = "D:\Dev\Tools\Inno"`. If ISCC is not found, the `installer` target is simply not created.
- The Inno Setup script is generated from `installer/setup.iss.in` by `configure_file()` into `<build-dir>/setup.iss`; version and arch are injected automatically.
- The installer handles VC++ Runtime detection, graceful app shutdown (IPC → `WM_CLOSE` → `taskkill`), and preserves user `config.json` across reinstall/uninstall.

## Build Targets

| Target | Output | Description |
|---|---|---|
| `Win_Switcher` | `AltTaber.exe` | Main application (`WIN32_EXECUTABLE`, no console) |
| `AltTaberCtl` | `AltTaberCtl.exe` | CLI control tool (sends `ping`/`quit` via IPC) |
| `test_filter` | `test_filter.exe` | `WindowFilter` unit tests |
| `test_hotkey` | `test_hotkey.exe` | `HotkeyConflictResolver` unit tests |
| `test_overlay` | `test_overlay.exe` | `OverlayController` state machine tests |
| `deploy` | — | Runs `windeployqt` on `AltTaber.exe` |
| `zip` | portable zip | Packages the build output directory |
| `installer` | setup exe | Compiles the Inno Setup installer |

## Versioning

- Project version lives in the root `CMakeLists.txt` (`project(Win_Switcher VERSION x.y.z)`).
- It is injected into:
  - `VersionInfo.rc` (Windows VERSIONINFO resource) from `VersionInfo.rc.in`
  - `setup.iss` (installer) from `installer/setup.iss.in`
  - Compile definitions `APP_VERSION` / `APP_ARCH`

## Notes

- Qt MOC/RCC/UIC run automatically (`AUTOMOC`/`AUTORCC`/`AUTOUIC`); `.ui` files are found via `CMAKE_AUTOUIC_SEARCH_PATHS` pointing at `ui/`.
- A `clangd` compilation database can be generated in `build/clangd` (see `.clangd`).
- If the app icon does not refresh after changing `img/icon.ico`, clear `%LOCALAPPDATA%\IconCache.db` and restart Explorer.
