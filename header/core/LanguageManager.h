#ifndef WIN_SWITCHER_LANGUAGEMANAGER_H
#define WIN_SWITCHER_LANGUAGEMANAGER_H

#include <QString>

QString detectSystemLanguage();
void switchLanguage(const QString& langCode);
void initLanguage();

#endif
