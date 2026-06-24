# AltTaber

![GitHub release (latest by date)](https://img.shields.io/github/v/release/MrBeanCpp/AltTaber)
![Github Release Downloads](https://img.shields.io/github/downloads/MrBeanCpp/AltTaber/total)
![Language](https://img.shields.io/badge/language-C++20-239120)
![OS](https://img.shields.io/badge/OS-Windows%20x64%20|%20ARM64-0078D4)
![Qt](https://img.shields.io/badge/Qt-6.8-41CD52)
![license](https://img.shields.io/github/license/MrBeanCpp/AltTaber)
![CI](https://img.shields.io/github/actions/workflow/status/MrBeanCpp/AltTaber/build.yml?branch=main)

**AltTaber** — A macOS-style Alt+Tab window/app switcher for Windows, built with Qt 6 / C++20.

![ui](../img/ui.png)

[中文](../README.md) [en](README.en.md)

---

## ✨ Features

### 1. `Alt+Tab` — Quick App Switching

Hold `Alt` and press `Tab` to browse all running applications. Release `Alt` to switch to the selected app. **MRU (Most Recently Used)** ordering keeps frequently used apps at the front, so you spend less time hunting. Hold `Shift` to reverse direction.

![switch apps](../img/Alt_tab.gif)

### 2. `Alt+\`` — Cycle Windows Within the Same App

Have multiple Chrome windows, VS Code projects, or File Explorer windows open? `Alt+Tab` only gets you to the app level — `Alt+\`` cycles directly through all open windows of the current app, saving you repeated `Alt+Tab` presses. The **most recently active** window comes first.

![switch windows](../img/Alt_`.gif)

### 3. 🖱️ Mouse Wheel Window Switching

With the switcher visible, hover over an app and scroll the mouse wheel to cycle its windows:
- **Scroll up**: Switch to previous window (without changing focus position)
- **Scroll down**: Minimize the current window

No extra keys needed — smooth and intuitive.

![wheel](../img/Alt_Wheel.gif)

### 4. 🖱️ Taskbar Wheel Cycling 🚧[Beta]🚧

No need to open the switcher — just scroll on any taskbar app icon to cycle through its windows. Great for quickly flipping through multiple windows of the same app.

- **Scroll up**: Switch to previous window
- **Scroll down**: Minimize current window

> **Caution**: Experimental — may not work with all applications.

![taskbar wheel](../img/Taskbar_Wheel.gif)

---

## 🌟 Highlights

- **Acrylic blur** background effect
  - ![bg blur](../img/bg-blur.png)
- **Win11 rounded corners**
- **Window badge** — shows window count per app
  - ![app badge](../img/app%20badge.png)
- **QQ avatar** — shows current chat friend's avatar on QQ icon
  - ![qq avatar](../img/app%20qq%20avatar.png)
- Configurable display monitor — follow mouse or fixed to primary screen
- Fully customizable hotkeys
- Letter Jump — jump to an app by typing its name's first letter
- PWA (Progressive Web App) window support

---

## 🔑 Run as Administrator (Optional)

Without admin rights, AltTaber cannot see elevated windows (e.g. Task Manager, game boosters).

---

## 🤖 Build from Source

### Prerequisites

| Requirement | Version |
|---|---|
| CMake | ≥ 3.29 |
| MSVC | 2022 |
| Qt | 6.8+ (Core, Gui, Widgets, Xml, Network, LinguistTools) |
| Windows SDK | 10.0+ |
| Inno Setup | 6+ (required for installer packaging) |

### Debug build

Build only the executable:
```bash
cmake --preset "release-x64"
cmake --build "build/x64"
```

Package portable zip:
```bash
cmake --preset "release-x64"
cmake --build "build/x64" --target zip
```

Package installer (requires Inno Setup 6+):
```bash
cmake --preset "release-x64"
cmake --build "build/x64" --target installer
```

---

## 📦 Download

Get the latest release from [GitHub Releases](https://github.com/Hendrix4858/AltTaber/releases).

---

## 🧐 References

- [window-switcher](https://github.com/sigoden/window-switcher)
- [cmdtab](https://github.com/stianhoiland/cmdtab)
- [Inno-Setup-Chinese-Simplified-Translation](https://github.com/kira-96/Inno-Setup-Chinese-Simplified-Translation)
