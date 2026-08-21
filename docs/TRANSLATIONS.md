# Translations

How AltTaber's i18n works, how to update existing translations, and how to add a new language.

[中文](../README.md) [en](README.en.md) | [构建](BUILD_CN.md) · [翻译](TRANSLATIONS_CN.md)　|　[Build](BUILD.md) · [Translations](TRANSLATIONS.md)

---

## Overview

- Currently supported language packs: **Simplified Chinese (`zh_CN`)**. English is the source language and needs no translation file.
- Translated strings are compiled into the binary via the [Qt resource system](#cmake-integration) — no separate `.qm` files need to be shipped.
- Language selection is stored in the config (`ConfigManager::getLanguage()`), with a `system` option that auto-detects at runtime.

## Workflow at a Glance

```
tr("...") in C++ sources
        │
        │  qt_add_lupdate (scans .cpp/.h/.ui, runs automatically on build)
        ▼
translations/zh_CN.ts          (XML, human-editable)
        │
        │  qt_add_lrelease (compiles TS → QM)
        ▼
zh_CN.qm                       (binary)
        │
        │  qt_add_resources (embedded into the executable)
        ▼
:/translations/zh_CN.qm        (loaded by LanguageManager at runtime)
```

All three steps are wired into the `Win_Switcher` target as normal build dependencies — building the app refreshes `.ts` and recompiles `.qm` automatically. There is no manual `lupdate`/`lrelease` invocation.

## Updating Existing Translations

1. Add or modify user-visible strings in source using `tr("...")` (see [String Conventions](#string-conventions)).
2. Build the project — `qt_add_lupdate` merges new/removed strings into `translations/zh_CN.ts`. Removed strings are marked as vanished, not deleted.
3. Open `translations/zh_CN.ts` in **Qt Linguist** (part of Qt installs, under `Tools/Qt Linguist`) or edit the XML directly.
4. Translate the entries marked *unfinished*; save.
5. Rebuild — the updated `.qm` is embedded into `AltTaber.exe`.
6. Verify in-app: switch language to 简体中文 in Settings (or set system locale to Chinese) and check the affected UI.

> Tip: `lupdate` is configured with `-locations relative`, so `.ts` line references stay portable across machines.

## Adding a New Language

Example: adding Traditional Chinese (`zh_TW`).

1. **Create the TS file** — copy the existing one to keep finished entries:
   ```shell
   cp translations/zh_CN.ts translations/zh_TW.ts
   ```
2. **Register it in `translations/CMakeLists.txt`:**
   ```cmake
   qt_add_lupdate(Win_Switcher TS_FILES zh_CN.ts zh_TW.ts
       OPTIONS -locations relative)
   qt_add_lrelease(Win_Switcher TS_FILES zh_CN.ts zh_TW.ts QM_FILES_OUTPUT_VARIABLE qm_files)
   # Give each QM a stable resource alias:
   set_source_files_properties(${qm_files} PROPERTIES QT_RESOURCE_ALIAS "zh_CN.qm") # adjust per-file, see note
   qt_add_resources(Win_Switcher "translations" PREFIX "/translations" FILES ${qm_files})
   ```
   With multiple TS files, set `QT_RESOURCE_ALIAS` per file so each `.qm` keeps its locale name inside the resource system.
3. **Register it in `LanguageManager`** (`src/core/LanguageManager.cpp`):
   - Extend `detectSystemLanguage()` mapping (e.g. `zh_TW`/`zh_HK` locales → `"zh_TW"`).
   - Extend `switchLanguage()` to load `:/translations/zh_TW.qm` for that code.
4. **Add the language option to the settings UI** (`SettingsDialog`) so users can pick it.
5. Translate the strings in Qt Linguist, rebuild, and test.

## Architecture

### LanguageManager

`src/core/LanguageManager.{h,cpp}` owns a global `QTranslator` and exposes:

| Function | Behavior |
|---|---|
| `detectSystemLanguage()` | `QLocale::system().name()` starting with `zh` → `zh_CN`, otherwise `en` |
| `initLanguage()` | Reads `cfg().getLanguage()` and calls `switchLanguage()`; invoked early in `Application::run()` |
| `switchLanguage(langCode)` | Removes the old translator → resolves `system` → loads `:/translations/<lang>.qm` → installs translator → `sysTray().retranslateMenu()` |

Design notes:

- Only one `QTranslator` instance is used; switching languages swaps its payload, keeping install/uninstall symmetric.
- Untranslated strings fall through to the source text (English), so a partial translation never breaks the UI.
- The tray menu is retranslated explicitly because `QAction` texts do not refresh automatically on translator changes.

### CMake Integration

`translations/CMakeLists.txt` (executed as part of the `Win_Switcher` target):

```cmake
qt_add_lupdate(Win_Switcher TS_FILES zh_CN.ts
    OPTIONS -locations relative)                     # scan sources -> update .ts
qt_add_lrelease(Win_Switcher TS_FILES zh_CN.ts
    QM_FILES_OUTPUT_VARIABLE qm_files)               # compile .ts -> .qm
set_source_files_properties(${qm_files} PROPERTIES
    QT_RESOURCE_ALIAS "zh_CN.qm")                    # resource path: /translations/zh_CN.qm
qt_add_resources(Win_Switcher "translations"
    PREFIX "/translations" FILES ${qm_files})        # embed into the exe
```

Note that `windeployqt` runs with `--no-translations`: Qt's own translation files are intentionally not deployed; only app translations embedded via resources are used.

### String Conventions

- Inside `QObject`-derived classes use `tr("...")`; the class name becomes the translation *context*.
- Outside a `QObject` context, call `QCoreApplication::translate("Context", "...")` explicitly, e.g. `QCoreApplication::translate("Application", "Exit")`.
- Contexts in `zh_CN.ts` map 1:1 to C++ class names (`SettingsDialog`, `SystemTray`, `HotkeyAction`, ...). Keep context names stable — renaming a class orphans its translations.
- Use `%1`/`%2` placeholders for dynamic values (`tr("Exported %1 rules.")`) instead of string concatenation, so word order can differ per language.

## File Reference

| File | Purpose |
|---|---|
| `translations/CMakeLists.txt` | lupdate/lrelease/resource wiring |
| `translations/zh_CN.ts` | Simplified Chinese translation source (edit this) |
| `src/core/LanguageManager.h/.cpp` | Language detection & runtime switching |
| `src/core/ConfigManager.*` | Persists the selected language |
| `src/lifecycle/SystemTray.cpp` | `retranslateMenu()` — tray menu retranslation |
