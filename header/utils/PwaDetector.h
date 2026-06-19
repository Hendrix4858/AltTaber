#ifndef WIN_SWITCHER_PWADETECTOR_H
#define WIN_SWITCHER_PWADETECTOR_H

#include <Windows.h>
#include <QString>
#include <QIcon>

namespace PwaDetector {

    enum class PwaType {
        None,
        ChromiumCrx,
        WindowsAppModel
    };

    QString getAppUserModelId(HWND hwnd);
    PwaType detectPwaType(const QString& processPath, const QString& appUserModelId);
    bool isPwaWindow(const QString& processPath, const QString& appUserModelId);
    QIcon getPwaIcon(HWND hwnd, const QString& appUserModelId, const QString& fallbackExePath);
    QString getPwaDisplayName(const QString& appUserModelId);
}

#endif
