# 构建指南

从源码构建、测试和打包 AltTaber 的开发者文档。

[中文](../README.md) [en](README.en.md) | [构建](BUILD_CN.md) · [翻译](TRANSLATIONS_CN.md)　|　[Build](BUILD.md) · [Translations](TRANSLATIONS.md)

---

## 前提条件

| 要求 | 版本 | 备注 |
|---|---|---|
| Windows | 10+ | 仅支持 Windows |
| CMake | ≥ 3.29 | |
| MSVC | VS 2022 (17) | C++20，全局启用 `/utf-8` |
| Qt | 6.8+ | 组件：`Core`、`Gui`、`Widgets`、`Xml`、`Network`、`LinguistTools` |
| Windows SDK | 10.0+ | 随 Visual Studio 安装 |
| Inno Setup | 6.5+ | 可选 — 仅 `installer` 目标需要 |
| windeployqt | 随 Qt 发行 | 可选 — `deploy` / `zip` / `installer` 目标需要 |

链接库：`Dwmapi`、`Propsys`、`windowsapp`、`shlwapi`（均随 Windows SDK 提供）。

## Qt 路径配置

Qt 通过环境变量检测，按以下顺序查找：

**x64 构建：**

1. `QT_DIR_X64` → 作为 `QT_DIR` 使用
2. `QT_DIR`
3. `QT_ROOT_DIR`

**ARM64 构建：**

1. `QT_DIR_ARM64`
2. `QT_DIR`（将其中 `msvc2022_64` 替换为 `msvc2022_arm64`）
3. `QT_ROOT_DIR`

示例（PowerShell）：

```powershell
$env:QT_DIR_X64   = "D:\Dev\IDE\Qt\6.8.3\msvc2022_64"
$env:QT_DIR_ARM64 = "D:\Dev\IDE\Qt\6.8.3\msvc2022_arm64"
```

## 构建 Preset

定义在 `CMakePresets.json` 中：

| Configure preset | Generator | 输出目录 | 配置 |
|---|---|---|---|
| `release-x64` | Visual Studio 17 2022 | `build/x64` | Release |
| `release-arm64` | Visual Studio 17 2022 | `build/arm64` | Release |
| `ninja-x64` | Ninja Multi-Config | `build/ninja-x64` | Debug + Release |
| `ninja-arm64` | Ninja Multi-Config | `build/ninja-arm64` | Debug + Release |

Build preset：`release-x64`、`release-arm64`、`ninja-x64-debug`、`ninja-x64-release`、`ninja-arm64-debug`、`ninja-arm64-release`。

> Ninja preset 需要从匹配架构的 **Native Tools Command Prompt**（或先调用 `vcvars64.bat` / `vcvarsamd64_arm64.bat`）运行，以确保 `cl.exe` 和 `ninja` 在 `PATH` 中。

## 常用命令

```shell
# 配置 + 构建（VS generator）
cmake --preset release-x64
cmake --build build/x64 --config Release

# 配置 + 构建（Ninja）
cmake --preset ninja-x64
cmake --build build/ninja-x64 --config Release        # 或 Debug

# 运行单元测试
cmake --build build/ninja-x64 --config Debug --target test_filter test_hotkey test_overlay
ctest --test-dir build/ninja-x64 -C Debug

# 部署 Qt 运行时 DLL 到 AltTaber.exe 旁边
cmake --build build/x64 --target deploy

# 打包便携 ZIP → build/output/AltTaber-<version>-<arch>.zip
cmake --build build/x64 --target zip

# 打包安装程序 → build/output/AltTaber-v<version>-<arch>-Setup.exe
cmake --build build/x64 --target installer
```

`zip` 和 `installer` 目标会自动依赖 `Win_Switcher`、`AltTaberCtl`，以及 `deploy`（当找到 `windeployqt` 时），因此单次构建目标即可产出可分发的产物。

### 安装程序注意事项

- 配置前设置 `ISCC_DIR` 环境变量指向 Inno Setup 目录（包含 `ISCC.exe`），例如 `$env:ISCC_DIR = "D:\Dev\Tools\Inno"`。若未找到 ISCC，则不会创建 `installer` 目标。
- Inno Setup 脚本由 `installer/setup.iss.in` 经 `configure_file()` 生成为 `<build-dir>/setup.iss`，版本号和架构自动注入。
- 安装脚本负责 VC++ Runtime 检测、优雅关闭应用（IPC → `WM_CLOSE` → `taskkill`）、以及在重装/卸载时保留用户 `config.json`。

## 构建目标一览

| 目标 | 输出 | 说明 |
|---|---|---|
| `Win_Switcher` | `AltTaber.exe` | 主程序（`WIN32_EXECUTABLE`，无控制台） |
| `AltTaberCtl` | `AltTaberCtl.exe` | CLI 控制工具（通过 IPC 发送 `ping`/`quit`） |
| `test_filter` | `test_filter.exe` | `WindowFilter` 单元测试 |
| `test_hotkey` | `test_hotkey.exe` | `HotkeyConflictResolver` 单元测试 |
| `test_overlay` | `test_overlay.exe` | `OverlayController` 状态机测试 |
| `deploy` | — | 对 `AltTaber.exe` 运行 `windeployqt` |
| `zip` | 便携 ZIP | 打包构建输出目录 |
| `installer` | 安装程序 exe | 编译 Inno Setup 安装包 |

## 版本管理

- 项目版本号位于根 `CMakeLists.txt`（`project(Win_Switcher VERSION x.y.z)`）。
- 注入到以下位置：
  - `VersionInfo.rc`（Windows VERSIONINFO 资源），来自 `VersionInfo.rc.in`
  - `setup.iss`（安装脚本），来自 `installer/setup.iss.in`
  - 编译定义 `APP_VERSION` / `APP_ARCH`

## 补充说明

- Qt MOC/RCC/UIC 自动运行（`AUTOMOC`/`AUTORCC`/`AUTOUIC` 已开启）；`.ui` 文件通过 `CMAKE_AUTOUIC_SEARCH_PATHS` 指向的 `ui/` 目录搜索。
- 可在 `build/clangd` 生成 `clangd` 编译数据库（见 `.clangd` 配置）。
- 若修改 `img/icon.ico` 后应用图标未刷新，需清除 `%LOCALAPPDATA%\IconCache.db` 并重启资源管理器。
