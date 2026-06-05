#include "utils/IconUtil.h"
#include <QDebug>
#include <QFileInfo>
#include <QFileIconProvider>
#include <QPainter>
#include <QElapsedTimer>
#include "utils/QtWin.h"
#include "utils/AppUtil.h"
#include <ShObjIdl_core.h>
#include <commoncontrols.h>
#include <atlbase.h>
#include <minappmodel.h>
#include <appmodel.h>
#include <shlobj_core.h>
#include <winrt/Windows.Management.Deployment.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.ApplicationModel.h>

namespace Util {
    QIcon getJumboIcon(const QString& filePath) {
        SHFILEINFOW sfi = {nullptr};
        SHGetFileInfo(filePath.toStdWString().c_str(), 0, &sfi, sizeof(SHFILEINFOW), SHGFI_SYSICONINDEX);

        IImageList* imageList;
        HRESULT hResult = SHGetImageList(SHIL_JUMBO, IID_IImageList, (void**) &imageList);

        QIcon icon;
        if (hResult == S_OK) {
            HICON hIcon;
            hResult = imageList->GetIcon(sfi.iIcon, ILD_TRANSPARENT, &hIcon);

            if (hResult == S_OK) {
                icon = QtWin::fromHICON(hIcon);
                DestroyIcon(sfi.hIcon);
            }
        }
        imageList->Release();
        return icon;
    }

    bool isBottomRightTransparent(const QIcon& icon, int extent) {
        QImage image = icon.pixmap(extent).toImage().convertToFormat(QImage::Format_ARGB32);

        int width = image.width();
        int height = image.height();

        for (int y = height / 2; y < height; ++y) {
            for (int x = width / 2; x < width; ++x) {
                if (qAlpha(image.pixel(x, y)) != 0)
                    return false;
            }
        }
        return true;
    }

    QString getUwpInstallDirFromHwnd(HWND hwnd) {
        if (AppUtil::isAppFrameWindow(hwnd))
            hwnd = AppUtil::getAppCoreWindow(hwnd);

        DWORD pid;
        GetWindowThreadProcessId(hwnd, &pid);
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!hProcess) {
            qWarning() << "OpenProcess failed" << GetLastError();
            return {};
        }

        WCHAR packageFullName[PACKAGE_FULL_NAME_MAX_LENGTH + 1] = {0};
        UINT32 length = _countof(packageFullName);
        if (auto result = GetPackageFullName(hProcess, &length, packageFullName); result != ERROR_SUCCESS) {
            if (result == ERROR_INSUFFICIENT_BUFFER)
                qWarning() << "Buffer too small for packageFullName";
            CloseHandle(hProcess);
            return {};
        }
        CloseHandle(hProcess);

        using namespace winrt;
        using namespace Windows::Management::Deployment;

        try {
            PackageManager packageManager;
            auto package = packageManager.FindPackageForUser(L"", hstring(packageFullName));
            if (!package) {
                qDebug() << "Package not found?";
                return {};
            }
            return QString::fromStdWString(package.InstalledPath().c_str());
        } catch (const hresult_error& ex) {
            qWarning() << "PackageManager Error:" << QString::fromStdWString(ex.message().c_str());
        }

        return {};
    }

    QIcon getCachedIcon(const QString& path, HWND hwnd) {
        static QHash<QString, QIcon> IconCache;
        if (auto icon = IconCache.value(path); !icon.isNull())
            return icon;

        QElapsedTimer t;
        t.start();
        QIcon icon;

        if (auto uwpDir = getUwpInstallDirFromHwnd(hwnd); !uwpDir.isEmpty()) {
            qDebug() << "Detect UWP" << path;
            icon = AppUtil::getAppIcon(uwpDir + "\\fake.exe");
        } else {
            icon = getJumboIcon(path);
            if (isBottomRightTransparent(icon, 32)) {
                qInfo() << "-- BottomRight is transparent, fallback to 48x48" << path;
                icon = QFileIconProvider().icon(QFileInfo(path)).pixmap(48);
            }
        }
        IconCache.insert(path, icon);
        qInfo() << "Icon not found in cache, loaded in" << t.elapsed() << "ms" << path;
        return icon;
    }

    QPixmap getWindowIcon(HWND hwnd) {
        auto hIcon = reinterpret_cast<HICON>(SendMessageW(hwnd, WM_GETICON, ICON_BIG, 0));
        if (!hIcon)
            hIcon = reinterpret_cast<HICON>(GetClassLongPtr(hwnd, GCLP_HICON));
        return QtWin::fromHICON(hIcon);
    }

    QIcon overlayIcon(const QPixmap& icon, const QPixmap& overlay, const QRect& overlayRect) {
        QPixmap bg = icon;
        QPainter painter(&bg);
        painter.drawPixmap(overlayRect, overlay);
        painter.end();
        return bg;
    }
}
