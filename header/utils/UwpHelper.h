#ifndef WIN_SWITCHER_UWPHELPER_H
#define WIN_SWITCHER_UWPHELPER_H

#include <Windows.h>
#include <QIcon>
#include <QString>

namespace AppUtil {
    HWND getAppFrameWindow(HWND hwnd);
    HWND getAppCoreWindow(HWND hwnd);
    bool isAppFrameWindow(HWND hwnd);
    QIcon getAppIcon(const QString& path);
    QString getUWPInstallDirByAUMID(const QString& AUMID);
    QString getUwpExePathByAUMID(const QString& AUMID);

    inline const QString AppCoreWindowClass = "Windows.UI.Core.CoreWindow";
    inline const QString AppFrameWindowClass = "ApplicationFrameWindow";
    inline const QString AppManifest = "AppxManifest.xml";
}

#endif //WIN_SWITCHER_UWPHELPER_H
