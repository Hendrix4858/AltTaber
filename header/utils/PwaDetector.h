#ifndef WIN_SWITCHER_PWADETECTOR_H
#define WIN_SWITCHER_PWADETECTOR_H

#include <Windows.h>
#include <QString>
#include <QIcon>

namespace PwaDetector {
    QString getAppUserModelId(HWND hwnd);
    bool isPwaWindow(const QString& processPath, const QString& appUserModelId);
    QIcon getPwaIcon(HWND hwnd, const QString& appUserModelId, const QString& fallbackExePath);
}

#endif
