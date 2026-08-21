# 翻译系统

AltTaber 的 i18n 工作原理、如何更新现有翻译、以及如何添加新语言。

[中文](../README.md) [en](README.en.md) | [构建](BUILD_CN.md) · [翻译](TRANSLATIONS_CN.md)　|　[Build](BUILD.md) · [Translations](TRANSLATIONS.md)

---

## 概述

- 当前支持的语言包：**简体中文（`zh_CN`）**。英文是源语言，无需翻译文件。
- 翻译字符串通过 [Qt 资源系统](#cmake-集成) 编译进二进制文件 — 无需单独分发 `.qm` 文件。
- 语言选择保存在配置中（`ConfigManager::getLanguage()`），并提供 `system` 选项在运行时自动检测。

## 工作流一览

```
C++ 源码中的 tr("...")
        │
        │  qt_add_lupdate（扫描 .cpp/.h/.ui，构建时自动运行）
        ▼
translations/zh_CN.ts          （XML，人类可编辑）
        │
        │  qt_add_lrelease（编译 TS → QM）
        ▼
zh_CN.qm                       （二进制）
        │
        │  qt_add_resources（嵌入可执行文件）
        ▼
:/translations/zh_CN.qm        （LanguageManager 运行时加载）
```

三个步骤均作为 `Win_Switcher` 目标的常规构建依赖接入 — 构建应用即自动刷新 `.ts` 并重新编译 `.qm`，无需手动调用 `lupdate`/`lrelease`。

## 更新现有翻译

1. 在源码中使用 `tr("...")` 新增或修改用户可见字符串（见[字符串规范](#字符串规范)）。
2. 构建项目 — `qt_add_lupdate` 会将新增/删除的字符串合并到 `translations/zh_CN.ts`。被删除的字符串标记为 vanished，不会直接移除。
3. 用 **Qt Linguist**（Qt 安装自带，位于 `Tools/Qt Linguist`）打开 `translations/zh_CN.ts`，或直接编辑 XML。
4. 翻译标记为 *unfinished* 的条目并保存。
5. 重新构建 — 更新后的 `.qm` 会嵌入 `AltTaber.exe`。
6. 应用内验证：在设置中切换语言为简体中文（或将系统区域设为中文），检查相关 UI。

> 提示：`lupdate` 配置了 `-locations relative`，因此 `.ts` 中的行号引用在不同机器间保持可移植。

## 添加新语言

以繁体中文（`zh_TW`）为例。

1. **创建 TS 文件** — 复制现有文件以保留已完成的条目：
   ```shell
   cp translations/zh_CN.ts translations/zh_TW.ts
   ```
2. **在 `translations/CMakeLists.txt` 中注册：**
   ```cmake
   qt_add_lupdate(Win_Switcher TS_FILES zh_CN.ts zh_TW.ts
       OPTIONS -locations relative)
   qt_add_lrelease(Win_Switcher TS_FILES zh_CN.ts zh_TW.ts QM_FILES_OUTPUT_VARIABLE qm_files)
   # 为每个 QM 设置稳定的资源别名：
   set_source_files_properties(${qm_files} PROPERTIES QT_RESOURCE_ALIAS "zh_CN.qm") # 多文件时需逐个设置，见下方说明
   qt_add_resources(Win_Switcher "translations" PREFIX "/translations" FILES ${qm_files})
   ```
   存在多个 TS 文件时，需逐个设置 `QT_RESOURCE_ALIAS`，确保每个 `.qm` 在资源系统中保留自己的 locale 名称。
3. **在 `LanguageManager` 中注册**（`src/core/LanguageManager.cpp`）：
   - 扩展 `detectSystemLanguage()` 的映射（如 `zh_TW`/`zh_HK` locale → `"zh_TW"`）。
   - 扩展 `switchLanguage()`，为该语言代码加载 `:/translations/zh_TW.qm`。
4. **在设置界面添加语言选项**（`SettingsDialog`），供用户选择。
5. 在 Qt Linguist 中完成翻译，重新构建并测试。

## 架构说明

### LanguageManager

`src/core/LanguageManager.{h,cpp}` 持有一个全局 `QTranslator`，对外暴露：

| 函数 | 行为 |
|---|---|
| `detectSystemLanguage()` | `QLocale::system().name()` 以 `zh` 开头 → `zh_CN`，否则 `en` |
| `initLanguage()` | 读取 `cfg().getLanguage()` 并调用 `switchLanguage()`；在 `Application::run()` 早期调用 |
| `switchLanguage(langCode)` | 移除旧 translator → 解析 `system` → 加载 `:/translations/<lang>.qm` → 安装 translator → `sysTray().retranslateMenu()` |

设计要点：

- 只使用一个 `QTranslator` 实例；切换语言即替换其内容，安装/卸载保持对称。
- 未翻译的字符串回退到源文本（英文），因此不完整的翻译不会破坏 UI。
- 托盘菜单需要显式重翻，因为 `QAction` 文本不会随 translator 变化自动刷新。

### CMake 集成

`translations/CMakeLists.txt`（作为 `Win_Switcher` 目标的一部分执行）：

```cmake
qt_add_lupdate(Win_Switcher TS_FILES zh_CN.ts
    OPTIONS -locations relative)                     # 扫描源码 -> 更新 .ts
qt_add_lrelease(Win_Switcher TS_FILES zh_CN.ts
    QM_FILES_OUTPUT_VARIABLE qm_files)               # 编译 .ts -> .qm
set_source_files_properties(${qm_files} PROPERTIES
    QT_RESOURCE_ALIAS "zh_CN.qm")                    # 资源路径：/translations/zh_CN.qm
qt_add_resources(Win_Switcher "translations"
    PREFIX "/translations" FILES ${qm_files})        # 嵌入可执行文件
```

注意 `windeployqt` 运行时带 `--no-translations`：有意不部署 Qt 自身的翻译文件，只使用通过资源嵌入的应用翻译。

### 字符串规范

- 在 `QObject` 派生类内使用 `tr("...")`；类名即为翻译 *context*。
- 在非 `QObject` 上下文中，显式调用 `QCoreApplication::translate("Context", "...")`，例如 `QCoreApplication::translate("Application", "Exit")`。
- `zh_CN.ts` 中的 context 与 C++ 类名一一对应（`SettingsDialog`、`SystemTray`、`HotkeyAction` 等）。保持 context 名称稳定 — 重命名类会导致其已有翻译失效。
- 动态值使用 `%1`/`%2` 占位符（`tr("Exported %1 rules.")`），不要用字符串拼接，以便不同语言可以调整语序。

## 文件参考

| 文件 | 用途 |
|---|---|
| `translations/CMakeLists.txt` | lupdate/lrelease/资源接入 |
| `translations/zh_CN.ts` | 简体中文翻译源文件（编辑此文件） |
| `src/core/LanguageManager.h/.cpp` | 语言检测与运行时切换 |
| `src/core/ConfigManager.*` | 持久化所选语言 |
| `src/lifecycle/SystemTray.cpp` | `retranslateMenu()` — 托盘菜单重翻 |
