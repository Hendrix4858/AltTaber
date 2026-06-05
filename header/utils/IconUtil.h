#ifndef WIN_SWITCHER_ICONUTIL_H
#define WIN_SWITCHER_ICONUTIL_H

#include <Windows.h>
#include <QString>
#include <QIcon>
#include <QPixmap>
#include <QRect>

namespace Util {
    QIcon getJumboIcon(const QString& filePath);
    QIcon getCachedIcon(const QString& path, HWND hwnd);
    QPixmap getWindowIcon(HWND hwnd);
    QIcon overlayIcon(const QPixmap& icon, const QPixmap& overlay, const QRect& overlayRect);
    QString getUwpInstallDirFromHwnd(HWND hwnd);
}

#endif //WIN_SWITCHER_ICONUTIL_H
