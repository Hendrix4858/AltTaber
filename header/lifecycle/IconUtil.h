#ifndef WIN_SWITCHER_ICONUTIL_H
#define WIN_SWITCHER_ICONUTIL_H

#include <Windows.h>
#include <QString>
#include <QIcon>
#include <QPixmap>
#include <QRect>

#include "WindowTypes.h"

namespace Util {
    QIcon getJumboIcon(const QString& filePath);
    QIcon getCachedIcon(const QString& path, HWND hwnd);
    QPixmap getWindowIcon(HWND hwnd);
    QIcon overlayIcon(const QPixmap& icon, const QPixmap& overlay, const QRect& overlayRect);
    QString getUwpInstallDirFromHwnd(HWND hwnd);
    QPixmap getShellAppIcon(HWND hwnd);
    QIcon getCachedPwaIcon(const QString& aumid);
    void cachePwaIcon(const QString& aumid, const QIcon& icon);
    QPixmap getIconFromAumid(const QString& aumid);
    QIcon resolveIdentityIcon(const AppIdentity& identity, HWND hwnd, const QString& fallbackExePath);
}

#endif //WIN_SWITCHER_ICONUTIL_H
