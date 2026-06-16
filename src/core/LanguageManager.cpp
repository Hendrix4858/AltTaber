#include "core/LanguageManager.h"
#include "core/ConfigManager.h"
#include "lifecycle/SystemTray.h"

#include <QApplication>
#include <QTranslator>
#include <QLocale>
#include <QDebug>

namespace {
QTranslator g_translator;
}

QString detectSystemLanguage() {
    QString sysLocale = QLocale::system().name();
    // Currently only has Simplified Chinese pack
    // Future: zh_TW -> zh_TW.qm, zh_HK -> zh_HK.qm
    if (sysLocale.startsWith("zh"))
        return "zh_CN";
    return "en";
}

void initLanguage() {
    QString lang = cfg().getLanguage();
    switchLanguage(lang);
}

void switchLanguage(const QString& langCode) {
    qApp->removeTranslator(&g_translator);

    QString targetLang = (langCode == "system") ? detectSystemLanguage() : langCode;

    if (targetLang == "zh_CN") {
        if (g_translator.load(":/translations/zh_CN.qm")) {
            qApp->installTranslator(&g_translator);
            qDebug().noquote() << "Loaded zh_CN translations";
        } else {
            qWarning() << "Failed to load zh_CN translations";
        }
    }

    sysTray().retranslateMenu();
}
