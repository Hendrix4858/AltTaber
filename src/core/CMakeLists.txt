target_sources(Win_Switcher PRIVATE
    ConfigManager.cpp               ConfigManager.h
    ConfigManagerBase.cpp           ConfigManagerBase.h
    HotkeyAction.cpp                HotkeyAction.h
    LanguageManager.cpp             LanguageManager.h
    StyleManager.cpp                StyleManager.h
    ThemeManager.cpp                ThemeManager.h
                                    IConfigStore.h
                                    HotkeyActionTypes.h
                                    HotkeyStrings.h
                                    HotkeyBinding.h
)
