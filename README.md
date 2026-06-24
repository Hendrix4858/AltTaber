# AltTaber

![GitHub release (latest by date)](https://img.shields.io/github/v/release/MrBeanCpp/AltTaber)
![Github Release Downloads](https://img.shields.io/github/downloads/MrBeanCpp/AltTaber/total)
![Language](https://img.shields.io/badge/language-C++20-239120)
![OS](https://img.shields.io/badge/OS-Windows%20x64%20|%20ARM64-0078D4)
![Qt](https://img.shields.io/badge/Qt-6.8-41CD52)
![license](https://img.shields.io/github/license/MrBeanCpp/AltTaber)
![CI](https://img.shields.io/github/actions/workflow/status/MrBeanCpp/AltTaber/build.yml?branch=main)

一款 macOS 风格的 `Alt+Tab` 窗口/应用切换器，专为 `Windows` 💻 打造。

![ui](img/ui.png)

[中文](README.md) [en](docs/README.en.md)

---
## ✨ 主要功能

### 1. `Alt+Tab` — 应用快速切换

按住 `Alt` 键并按 `Tab` 键，浏览所有正在运行的应用，松开 `Alt` 切换到目标应用.
使用 Windows Z-order 排序规则

![switch apps](img/Alt_tab.gif)

### 2. `Alt+\`` — 同一应用的多窗口轮换

一个应用开启多个窗口,可以直接使用 `alt + \`` 在同进程窗口间切换.

![switch windows](img/Alt_`.gif)

### 3. 🖱️ 鼠标滚轮切换窗口

切换器可见时，鼠标悬停在某个应用上，滚动滚轮即可在该应用的多个窗口中切换：
- **向上滚动**：切换到上一个窗口（不改变焦点位置）
- **向下滚动**：最小化当前窗口

无需额外按键，操作直观连贯。

![wheel](img/Alt_Wheel.gif)

### 4. 🖱️ 任务栏滚轮切换 🚧[Beta]🚧

无需呼出切换器，直接在任务栏的应用图标上滚动滚轮即可切换该应用的窗口。适合快速翻阅多个同应用窗口的场景。

- **向上滚动**：切换到上一个窗口
- **向下滚动**：最小化当前窗口

> **注意**：实验性功能，可能对部分应用程序失效。

![taskbar wheel](img/Taskbar_Wheel.gif)

---

## 🌟 更多特色

- 窗口背景**毛玻璃**特效
  - ![bg blur](img/bg-blur.png)
- 适配 `Win11` 的窗口圆角
- 在应用图标右上角显示**窗口数量**（Badge）
  - ![app badge](img/app%20badge.png)
- 在 `QQ` 🐧 图标右下角显示当前聊天好友`头像`
  - ![qq avatar](img/app%20qq%20avatar.png)
- 可切换跟随鼠标所在屏幕或固定主屏幕
- 自定义快捷键.
- 按字母跳转功能.
- 支持 PWA 应用窗口.

---
## 🔑 以管理员身份运行（可选）

普通用户权限下无法查看有管理员权限的窗口.

## 🤖 从源码构建

### 前置要求

| 依赖 | 版本 |
|------|------|
| CMake | ≥ 3.29 |
| MSVC | 2022 |
| Qt | 6.8+ (Core, Gui, Widgets, Xml, Network, LinguistTools) |
| Windows SDK | 10.0+ |
| INNO setup | Inno Setup 6+ |

### 调试构建
构建安装包需要使用 INNO setup 来打包.
```shell
# 只构建
cmake --preset "release-x64"
cmake --build "build/x64"

# 打包应用
cmake --preset "release-x64"
cmake --build "build/x64" --target installer

# 构建便携版
cmake --preset "release-x64"
cmake --build "build/x64" --target zip
```


---

## 📦 下载

从 [GitHub Releases](https://github.com/Hendrix4858/AltTaber/releases) 获取最新版本

---

## 🧐 参考

- [window-switcher](https://github.com/sigoden/window-switcher)
- [cmdtab](https://github.com/stianhoiland/cmdtab)
- [Inno-Setup-Chinese-Simplified-Translation](https://github.com/kira-96/Inno-Setup-Chinese-Simplified-Translation)