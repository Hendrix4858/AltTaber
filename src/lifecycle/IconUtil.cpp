#include "lifecycle/IconUtil.h"
#include "utils/PwaDetector.h"

#include <QDebug>
#include <QFileInfo>
#include <QFileIconProvider>
#include <QPainter>
#include <QElapsedTimer>
#include <QCryptographicHash>
#include <QDir>
#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QImage>
#include "lifecycle/QtWin.h"
#include "utils/AppUtil.h"
#include "utils/WindowUtil.h"
#include "core/ConfigManager.h"
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

        IImageList* imageList = nullptr;
        HRESULT hResult = SHGetImageList(SHIL_JUMBO, IID_IImageList, (void**) &imageList);

        QIcon icon;
        if (hResult == S_OK && imageList) {
            HICON hIcon;
            hResult = imageList->GetIcon(sfi.iIcon, ILD_TRANSPARENT, &hIcon);

            if (hResult == S_OK) {
                icon = QtWin::fromHICON(hIcon);
                DestroyIcon(hIcon);
            }
        }
        if (imageList) imageList->Release();
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
        auto originalHwnd = hwnd;
        HWND coreHwnd = nullptr;
        DWORD pid = 0;

        if (AppUtil::isAppFrameWindow(hwnd))
            coreHwnd = AppUtil::getAppCoreWindow(hwnd);

        if (coreHwnd) {
            GetWindowThreadProcessId(coreHwnd, &pid);
        } else if (AppUtil::isAppFrameWindow(originalHwnd)) {
            DWORD framePid = 0;
            GetWindowThreadProcessId(originalHwnd, &framePid);
            for (auto child : Util::enumChildWindows(originalHwnd)) {
                DWORD childPid = 0;
                GetWindowThreadProcessId(child, &childPid);
                if (childPid != 0 && childPid != framePid) {
                    pid = childPid;
                    break;
                }
            }
            if (!pid)
                GetWindowThreadProcessId(originalHwnd, &pid);
        } else {
            GetWindowThreadProcessId(originalHwnd, &pid);
        }

        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!hProcess) {
            qCritical() << "OpenProcess failed" << GetLastError();
            return {};
        }

        WCHAR packageFullName[PACKAGE_FULL_NAME_MAX_LENGTH + 1] = {0};
        UINT32 length = _countof(packageFullName);
        if (auto result = GetPackageFullName(hProcess, &length, packageFullName); result != ERROR_SUCCESS) {
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

    namespace {
        static QHash<QString, QIcon> g_pwaMemCache;

        struct CacheEntry {
            QString type;
            QString key;
            QString iconFile;
            QString exeModified;
        };

        static QString legacyPwaPath(const QString& aumid) {
            return cfg().getIconCacheDirectory() + "/icons/pwa_"
                + QCryptographicHash::hash(aumid.toUtf8(), QCryptographicHash::Md5).toHex()
                + ".png";
        }

        static QIcon loadLegacyPwaIcon(const QString& aumid) {
            auto path = legacyPwaPath(aumid);
            if (!QFile::exists(path))
                return {};
            QIcon icon(path);
            if (icon.isNull()) {
                QFile::remove(path);
                return {};
            }
            return icon;
        }

        static void removeLegacyPwaIcon(const QString& aumid) {
            QFile::remove(legacyPwaPath(aumid));
        }

        class DiskCache {
        public:
            static DiskCache& instance() {
                static DiskCache cache;
                return cache;
            }

            QIcon load(const QString& exePath) {
                if (!cfg().getIconCacheEnabled()) return {};
                ensureLoaded();

                for (const auto& e : m_entries) {
                    if (e.type != "exe" || e.key != exePath) continue;

                    QFileInfo fi(exePath);
                    if (!fi.exists()) return {};
                    if (fi.lastModified().toSecsSinceEpoch() != QDateTime::fromString(e.exeModified, Qt::ISODate).toSecsSinceEpoch()) {
                        QFile::remove(iconDir() + "/" + e.iconFile);
                        for (int i = 0; i < m_entries.size(); ++i) {
                            if (m_entries[i].type == "exe" && m_entries[i].key == exePath) {
                                m_entries.removeAt(i);
                                break;
                            }
                        }
                        save();
                        return {};
                    }

                    QPixmap pix;
                    if (pix.load(iconDir() + "/" + e.iconFile))
                        return QIcon(pix);
                }
                return {};
            }

            void save(const QString& exePath, const QIcon& icon) {
                if (!cfg().getIconCacheEnabled() || icon.isNull()) return;
                ensureLoaded();

                QDir().mkpath(iconDir());

                auto hash = QCryptographicHash::hash(exePath.toUtf8(), QCryptographicHash::Sha256).toHex();
                auto fileName = hash + ".png";
                auto filePath = iconDir() + "/" + fileName;

                for (int i = 0; i < m_entries.size(); ++i) {
                    if (m_entries[i].type == "exe" && m_entries[i].key == exePath) {
                        QFile::remove(iconDir() + "/" + m_entries[i].iconFile);
                        m_entries.removeAt(i);
                        break;
                    }
                }

                qDebug().nospace() << "[DiskCache::save] path=" << exePath << " availableSizes=" << icon.availableSizes() << " actualSize(64,64)=" << icon.actualSize(QSize(64, 64)) << " pixmap(64).size=" << icon.pixmap(64).size();
                icon.pixmap(64).save(filePath, "PNG");

                CacheEntry entry;
                entry.type = "exe";
                entry.key = exePath;
                entry.exeModified = QFileInfo(exePath).lastModified().toString(Qt::ISODate);
                entry.iconFile = fileName;
                m_entries.append(entry);

                save();
            }

            QIcon loadPwa(const QString& aumid) {
                if (!cfg().getIconCacheEnabled()) return {};
                ensureLoaded();

                for (const auto& e : m_entries) {
                    if (e.type != "pwa" || e.key != aumid) continue;

                    QPixmap pix;
                    if (pix.load(iconDir() + "/" + e.iconFile))
                        return QIcon(pix);
                }
                return {};
            }

            void savePwa(const QString& aumid, const QIcon& icon) {
                if (!cfg().getIconCacheEnabled() || icon.isNull()) return;
                ensureLoaded();

                QDir().mkpath(iconDir());

                for (int i = 0; i < m_entries.size(); ++i) {
                    if (m_entries[i].type == "pwa" && m_entries[i].key == aumid) {
                        QFile::remove(iconDir() + "/" + m_entries[i].iconFile);
                        m_entries.removeAt(i);
                        break;
                    }
                }

                auto hash = QCryptographicHash::hash(aumid.toUtf8(), QCryptographicHash::Md5).toHex();
                auto fileName = "pwa_" + hash + ".png";
                qDebug().nospace() << "[DiskCache::savePwa] aumid=" << aumid << " availableSizes=" << icon.availableSizes() << " actualSize(64,64)=" << icon.actualSize(QSize(64, 64)) << " pixmap(64).size=" << icon.pixmap(64).size();
                icon.pixmap(64).save(iconDir() + "/" + fileName, "PNG");

                CacheEntry entry;
                entry.type = "pwa";
                entry.key = aumid;
                entry.iconFile = fileName;
                m_entries.append(entry);

                save();
            }

        private:
            QList<CacheEntry> m_entries;
            bool m_loaded = false;

            void ensureLoaded() {
                if (m_loaded) return;
                m_loaded = true;

                QFile file(manifestPath());
                if (!file.open(QIODevice::ReadOnly)) return;
                auto doc = QJsonDocument::fromJson(file.readAll());
                auto arr = doc.object()["entries"].toArray();
                for (const auto& val : arr) {
                    auto obj = val.toObject();
                    CacheEntry e;
                    e.type = obj["type"].toString();
                    if (e.type.isEmpty())
                        e.type = "exe";
                    if (e.type == "exe") {
                        e.key = obj["key"].toString();
                        if (e.key.isEmpty())
                            e.key = obj["exe"].toString();
                        e.exeModified = obj["exeModified"].toString();
                    } else {
                        e.key = obj["key"].toString();
                    }
                    e.iconFile = obj["iconFile"].toString();
                    m_entries.append(e);
                }
            }

            void save() {
                QDir().mkpath(manifestDir());
                QJsonArray arr;
                for (const auto& e : m_entries) {
                    QJsonObject obj;
                    obj["type"] = e.type;
                    obj["key"] = e.key;
                    obj["iconFile"] = e.iconFile;
                    if (e.type == "exe")
                        obj["exeModified"] = e.exeModified;
                    arr.append(obj);
                }
                QJsonObject root;
                root["entries"] = arr;
                QFile file(manifestPath());
                if (file.open(QIODevice::WriteOnly))
                    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
            }

            static QString manifestDir() { return cfg().getIconCacheDirectory(); }
            static QString iconDir() { return manifestDir() + "/icons"; }
            static QString manifestPath() { return manifestDir() + "/manifest.json"; }
        };
    }

    QIcon getCachedIcon(const QString& path, HWND hwnd) {
        static QHash<QString, QIcon> IconCache;
        if (auto icon = IconCache.value(path); !icon.isNull())
            return icon;

        if (auto icon = DiskCache::instance().load(path); !icon.isNull()) {
            IconCache.insert(path, icon);
            return icon;
        }

        QElapsedTimer t;
        t.start();
        QIcon icon;

        if (auto uwpDir = getUwpInstallDirFromHwnd(hwnd); !uwpDir.isEmpty()) {
            icon = AppUtil::getAppIcon(uwpDir + "\\fake.exe");
        } else {
            icon = getJumboIcon(path);
            if (isBottomRightTransparent(icon, 32)) {
                icon = QFileIconProvider().icon(QFileInfo(path)).pixmap(48);
            }
        }

        DiskCache::instance().save(path, icon);
        IconCache.insert(path, icon);
        qDebug() << "Icon not found in cache, loaded in" << t.elapsed() << "ms" << path;
        return icon;
    }

    QIcon getCachedPwaIcon(const QString& aumid) {
        if (auto icon = g_pwaMemCache.value(aumid); !icon.isNull())
            return icon;

        if (auto icon = DiskCache::instance().loadPwa(aumid); !icon.isNull()) {
            g_pwaMemCache.insert(aumid, icon);
            return icon;
        }

        if (auto icon = loadLegacyPwaIcon(aumid); !icon.isNull()) {
            DiskCache::instance().savePwa(aumid, icon);
            removeLegacyPwaIcon(aumid);
            g_pwaMemCache.insert(aumid, icon);
            return icon;
        }

        return {};
    }

    void cachePwaIcon(const QString& aumid, const QIcon& icon) {
        if (icon.isNull()) return;
        DiskCache::instance().savePwa(aumid, icon);
        g_pwaMemCache.insert(aumid, icon);
    }

    QPixmap getWindowIcon(HWND hwnd) {
        auto hIcon = reinterpret_cast<HICON>(SendMessageW(hwnd, WM_GETICON, ICON_BIG, 0));
        if (!hIcon)
            hIcon = reinterpret_cast<HICON>(GetClassLongPtr(hwnd, GCLP_HICON));
        if (!hIcon) {
            qWarning() << "getWindowIcon: no icon found for hwnd" << hwnd;
            return {};
        }
        return QtWin::fromHICON(hIcon);
    }

    QPixmap getShellAppIcon(HWND hwnd) {
        QString aumid = PwaDetector::getAppUserModelId(hwnd);
        if (aumid.isEmpty()) return {};

        // AppsFolder namespace: shell:AppsFolder\<AUMID>
        QString path = QStringLiteral("shell:AppsFolder\\") + aumid;
        CComPtr<IShellItem> shellItem;
        HRESULT hr = SHCreateItemFromParsingName(
            reinterpret_cast<const wchar_t*>(path.utf16()),
            nullptr,
            IID_PPV_ARGS(&shellItem));
        if (FAILED(hr) || !shellItem) return {};

        CComPtr<IShellItemImageFactory> imgFactory;
        hr = shellItem->QueryInterface(IID_PPV_ARGS(&imgFactory));
        if (FAILED(hr) || !imgFactory) return {};

        HBITMAP hBitmap = nullptr;
        SIZE sz = {128, 128};
        hr = imgFactory->GetImage(sz, SIIGBF_BIGGERSIZEOK, &hBitmap);
        if (FAILED(hr) || !hBitmap) return {};

        QImage img;
        QImage::Format imgFormat = QImage::Format_Invalid;
        {
            BITMAP bm = {};
            GetObject(hBitmap, sizeof(bm), &bm);
            qDebug().nospace() << "[IconUtil::getShellAppIcon] HBITMAP size=" << bm.bmWidth << "x" << bm.bmHeight << " bpp=" << bm.bmBitsPixel;
            if (bm.bmBitsPixel == 32) {
                img = QImage(bm.bmWidth, bm.bmHeight, QImage::Format_ARGB32_Premultiplied);
                BITMAPINFO bmi = {};
                bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                bmi.bmiHeader.biWidth  = bm.bmWidth;
                bmi.bmiHeader.biHeight = -bm.bmHeight;
                bmi.bmiHeader.biPlanes = 1;
                bmi.bmiHeader.biBitCount = 32;
                bmi.bmiHeader.biCompression = BI_RGB;
                HDC hdc = GetDC(nullptr);
                if (hdc) {
                    GetDIBits(hdc, hBitmap, 0, bm.bmHeight, img.bits(), &bmi, DIB_RGB_COLORS);
                    ReleaseDC(nullptr, hdc);
                }
                imgFormat = QImage::Format_ARGB32_Premultiplied;
            } else {
                img = QImage::fromHBITMAP(hBitmap);
                imgFormat = img.format();
            }
        }
        DeleteObject(hBitmap);
        qDebug().nospace() << "[IconUtil::getShellAppIcon] QImage format=" << imgFormat << " hasAlpha=" << img.hasAlphaChannel() << " size=" << img.width() << "x" << img.height();
        QPixmap pix = QPixmap::fromImage(img);
        qDebug().nospace() << "[IconUtil::getShellAppIcon] result QPixmap size=" << pix.size() << " dpr=" << pix.devicePixelRatio();
        return pix;
    }

    QIcon overlayIcon(const QPixmap& icon, const QPixmap& overlay, const QRect& overlayRect) {
        QPixmap bg = icon;
        QPainter painter(&bg);
        painter.drawPixmap(overlayRect, overlay);
        painter.end();
        return bg;
    }
}
