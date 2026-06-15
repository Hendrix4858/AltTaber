#include "core/LanguageManager.h"
#include "core/ConfigManager.h"
#include "lifecycle/SystemTray.h"
#include "core/JsonTranslator.h"

#include <QApplication>
#include <QLocale>
#include <QDebug>

static JsonTranslator s_translator;

QString detectSystemLanguage() {
    QString sysLocale = QLocale::system().name();
    if (sysLocale.startsWith("zh"))
        return "zh_CN";
    return "en";
}

void initLanguage() {
    QString lang = cfg().getLanguage();
    switchLanguage(lang);
}

void switchLanguage(const QString& langCode) {
    qApp->removeTranslator(&s_translator);

    QString targetLang = (langCode == "system") ? detectSystemLanguage() : langCode;

    if (targetLang == "zh_CN") {
        if (s_translator.load(":/translations/zh_CN.json")) {
            qApp->installTranslator(&s_translator);
        }
    }

    sysTray().retranslateMenu();
}
