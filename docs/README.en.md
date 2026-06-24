# AltTaber

![GitHub release (latest by date)](https://img.shields.io/github/v/release/MrBeanCpp/AltTaber)
![Github Release Downloads](https://img.shields.io/github/downloads/MrBeanCpp/AltTaber/total)
![Language](https://img.shields.io/badge/language-C++20-239120)
![OS](https://img.shields.io/badge/OS-Windows%20x64%20|%20ARM64-0078D4)
![Qt](https://img.shields.io/badge/Qt-6.8-41CD52)
![license](https://img.shields.io/github/license/MrBeanCpp/AltTaber)
![CI](https://img.shields.io/github/actions/workflow/status/MrBeanCpp/AltTaber/build.yml?branch=main)

**AltTaber** — A macOS-style Alt+Tab window switcher for Windows 💻.

![ui](../img/ui.png)

[中文](../README.md) [en](README.en.md)

---
## ✨ Features

### 1. `Alt+Tab` — Quick App Switching

Hold `Alt` and press `Tab` to browse all running applications. Release `Alt` to switch to the selected app. Uses Windows Z-order sorting.

![switch apps](../img/Alt_tab.gif)

### 2. `Alt+\`` — Cycle windows within the same app

Open multiple windows in one app? Use `Alt+\`` to switch between windows of the same process.

![switch windows](../img/Alt_`.gif)

### 3. 🖱️ Mouse wheel window switching

With the switcher visible, hover over an app and scroll the mouse wheel to cycle its windows:
- **Scroll up**: Switch to previous window (no focus change)
- **Scroll down**: Minimize current window

No extra keys needed — smooth and intuitive.

![wheel](../img/Alt_Wheel.gif)

### 4. 🖱️ Taskbar wheel cycling 🚧[Beta]🚧

No need to open the switcher — just scroll on a taskbar app icon to cycle its windows. Great for quick multitasking.

- **Scroll up**: Switch to previous window
- **Scroll down**: Minimize current window

> **Caution**: Experimental — may not work with all applications.

![taskbar wheel](../img/Taskbar_Wheel.gif)

---

## 🌟 Highlights

- **Acrylic blur** background
  - ![bg blur](../img/bg-blur.png)
- **Win11 rounded corners**
- **Window badge** — shows window count per app
  - ![app badge](../img/app%20badge.png)
- **QQ avatar** — shows current chat friend's avatar on QQ icon
  - ![qq avatar](../img/app%20qq%20avatar.png)
- Configurable display monitor — follow mouse or fixed to primary screen
- Customizable hotkeys
- Letter Jump (A-Z)
- PWA (Progressive Web App) support

---
## 🔑 Run as Administrator (Optional)

Without admin rights, AltTaber cannot see elevated windows.

## 🤖 Build from Source

### Prerequisites

| Requirement | Version |
|---|---|
| CMake | ≥ 3.29 |
| MSVC | 2022 |
| Qt | 6.8+ (Core, Gui, Widgets, Xml, Network, LinguistTools) |
| Windows SDK | 10.0+ |
| Inno Setup | 6+ |

### Build
Inno Setup is required for installer packaging.
```shell
# Build only
cmake --preset "release-x64"
cmake --build "build/x64"

# Package installer
cmake --preset "release-x64"
cmake --build "build/x64" --target installer

# Package portable zip
cmake --preset "release-x64"
cmake --build "build/x64" --target zip
```


---

## 📦 Download

Get the latest release from [GitHub Releases](https://github.com/Hendrix4858/AltTaber/releases).

---

## 🧐 References

- [window-switcher](https://github.com/sigoden/window-switcher)
- [cmdtab](https://github.com/stianhoiland/cmdtab)
- [Inno-Setup-Chinese-Simplified-Translation](https://github.com/kira-96/Inno-Setup-Chinese-Simplified-Translation)
