#ifndef WIN_SWITCHER_ICONUTIL_H
#define WIN_SWITCHER_ICONUTIL_H

#include <Windows.h>
#include <QString>
#include <QIcon>
#include <QPixmap>
#include <QRect>

#include "WindowTypes.h"

enum class IconSource {
    None,
    Aumid,       // IShellItemImageFactory (256/128)
    ShellJumbo,  // SHIL_JUMBO (256)
    DefExtract,  // SHDefExtractIconW
    Window,      // WM_GETICON
    Provider,    // QFileIconProvider
};

struct IconResult {
    QIcon icon;
    QSize sourceSize;
    IconSource source = IconSource::None;
};

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

    // 按应用类型分场景选择图标来源，身份正确性优先于分辨率
    IconResult resolveWindowIcon(HWND hwnd, const QString& processPath,
                                 const QString& appUserModelId,
                                 WindowKind windowKind = WindowKind::Normal,
                                 const QString& processName = {},
                                 const AppIdentity& identity = {});
    IconResult resolveWindowIcon(const WindowDescriptor& desc);

    // 显示名称解析（独立于图标和身份识别）
    QString resolveDisplayName(const QString& pwaDisplayName,
                               const AppIdentity& identity,
                               const QString& title,
                               const QString& processPath,
                               WindowKind windowKind);
    QString resolveDisplayName(const WindowDescriptor& desc);
}

#endif //WIN_SWITCHER_ICONUTIL_H
