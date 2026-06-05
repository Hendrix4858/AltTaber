#include "utils/LanguageManager.h"
#include "utils/ConfigManager.h"
#include "utils/SystemTray.h"
#include "utils/JsonTranslator.h"

#include <QApplication>
#include <QDebug>

static JsonTranslator s_translator;

void initLanguage() {
    QString lang = cfg().getLanguage();
    switchLanguage(lang);
}

void switchLanguage(const QString& langCode) {
    qApp->removeTranslator(&s_translator);

    if (langCode == "zh_CN") {
        if (s_translator.load(":/translations/zh_CN.json")) {
            qApp->installTranslator(&s_translator);
        }
    }

    sysTray().retranslateMenu();
}
