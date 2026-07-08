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

#include "lifecycle/IconUtil.h"
#include "utils/PwaDetector.h"
#include "lifecycle/QtWin.h"
#include "utils/AppUtil.h"
#include "utils/MiscUtil.h"
#include "utils/WindowUtil.h"
#include "core/ConfigManager.h"
#include <ShObjIdl_core.h>
#include <commoncontrols.h>
#include <atlbase.h>
#include <minappmodel.h>
#include <appmodel.h>
#include <shlobj_core.h>
#include <shlwapi.h>
#include <winrt/Windows.Management.Deployment.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.ApplicationModel.h>
namespace Util {
    // ── 图标质量分析 & 评分（SHIL_JUMBO vs SHIL_EXTRALARGE 选优） ──
    struct IconAnalysis {
        bool valid = false;
        QPixmap image;
        int canvasW = 0, canvasH = 0;
        int contentX = 0, contentY = 0, contentW = 0, contentH = 0;
    };

    static IconAnalysis analyzeIcon(const QPixmap& pix) {
        IconAnalysis r;
        if (pix.isNull()) return r;
        r.valid = true;
        r.image = pix;
        r.canvasW = pix.width();
        r.canvasH = pix.height();
        if (r.canvasW <= 0 || r.canvasH <= 0) return r;

        QImage img = pix.toImage().convertToFormat(QImage::Format_ARGB32);
        int w = img.width(), h = img.height();
        int minX = w, minY = h, maxX = 0, maxY = 0;
        int step = (w > 128) ? 4 : 2;

        for (int y = 0; y < h; y += step) {
            const uchar* line = img.constScanLine(y);
            for (int x = 0; x < w; x += step) {
                if (line[x * 4 + 3] > 0) {
                    if (x < minX) minX = x;
                    if (y < minY) minY = y;
                    if (x > maxX) maxX = x;
                    if (y > maxY) maxY = y;
                }
            }
        }

        if (minX <= maxX) {
            r.contentX = minX;
            r.contentY = minY;
            r.contentW = maxX - minX + 1;
            r.contentH = maxY - minY + 1;
        }
        return r;
    }

    static bool isFallbackLayout(const IconAnalysis& a) {
        if (!a.valid || a.canvasW <= 0 || a.canvasH <= 0)
            return false;
        double cw = double(a.contentW) / a.canvasW;
        double ch = double(a.contentH) / a.canvasH;
        bool isSmallContent = (cw < 0.35 && ch < 0.35);
        bool isCornerAligned =
            (a.contentX < a.canvasW * 0.2 ||
             a.contentY < a.canvasH * 0.2 ||
             a.contentX + a.contentW > a.canvasW * 0.8 ||
             a.contentY + a.contentH > a.canvasH * 0.8);
        return isSmallContent && isCornerAligned;
    }

    // 内部辅助：提取原生尺寸 QPixmap，跳过 QIcon 间接层
    // 供 getJumboIcon 和 getCachedIcon 使用
    static QPixmap extractJumboIconPixmap(const QString& filePath) {
        auto wPath = filePath.toStdWString();

        // 单次 SHGetFileInfo，供各 ImageList 步骤共用
        SHFILEINFOW sfi{};
        int iIcon = -1;
        if (SHGetFileInfoW(wPath.c_str(), FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi),
                           SHGFI_SYSICONINDEX | SHGFI_USEFILEATTRIBUTES))
            iIcon = sfi.iIcon;

        // 从指定 shell 图像列表提取图标
        auto tryShellImageList = [&](int type) -> QPixmap {
            if (iIcon < 0) return {};
            IImageList* imageList = nullptr;
            if (FAILED(SHGetImageList(type, IID_IImageList, (void**)&imageList)))
                return {};
            HICON hIcon = nullptr;
            QPixmap pix;
            if (SUCCEEDED(imageList->GetIcon(iIcon, ILD_TRANSPARENT, &hIcon))) {
                pix = QtWin::fromHICON(hIcon);
                DestroyIcon(hIcon);
            }
            imageList->Release();
            return pix;
        };

        // 从 exe 资源直接提取图标
        auto tryDefExtract = [&](int size) -> QPixmap {
            HICON hIcon = nullptr;
            if (FAILED(SHDefExtractIconW(wPath.c_str(), 0, 0, &hIcon, nullptr, size))
                || !hIcon)
                return {};
            QPixmap pix = QtWin::fromHICON(hIcon);
            DestroyIcon(hIcon);
            return pix;
        };

        QString source;
        QPixmap pix;

        QPixmap jumbo, extra, d256, d48, d32;
        jumbo = tryShellImageList(SHIL_JUMBO);
        extra = tryShellImageList(SHIL_EXTRALARGE);

        auto jAnalysis = analyzeIcon(jumbo);

        if (!jumbo.isNull() && !isFallbackLayout(jAnalysis)) {
            pix = jumbo; source = QStringLiteral("SHIL_JUMBO");
        } else if (!extra.isNull()) {
            pix = extra; source = QStringLiteral("SHIL_EXTRALARGE");
        } else if (!(d256 = tryDefExtract(256)).isNull()) {
            pix = d256; source = QStringLiteral("DefExtract(256)");
        } else if (!(d48 = tryDefExtract(48)).isNull()) {
            pix = d48; source = QStringLiteral("DefExtract(48)");
        } else {
            pix = tryDefExtract(32);
            if (!pix.isNull()) source = QStringLiteral("DefExtract(32)");
        }

#ifndef NDEBUG
        qDebug().nospace() << "[extractJumboIconPixmap] path=" << filePath
                           << " source=" << source
                           << " size=" << pix.size()
                           << " [jumbo=" << jumbo.size()
                           << " extra=" << extra.size()
                           << " def256=" << d256.size()
                           << " def48=" << d48.size()
                           << " def32=" << d32.size()
                           << "]";
#endif
        return pix;
    }

    QIcon getJumboIcon(const QString& filePath) {
        QPixmap pix = extractJumboIconPixmap(filePath);
        return pix.isNull() ? QIcon() : QIcon(pix);
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

            void saveExeIcon(const QString& exePath, const QPixmap& pixmap) {
                if (!cfg().getIconCacheEnabled() || pixmap.isNull()) return;
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

                qDebug().nospace() << "[DiskCache::saveExeIcon] path=" << exePath
                                   << " size=" << pixmap.size();
                pixmap.save(filePath, "PNG");

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
                // Save original-size version for QIcon scaling comparison
                auto sizes = icon.availableSizes();
                if (!sizes.isEmpty()) {
                    auto maxSize = *std::max_element(sizes.begin(), sizes.end(), [](const QSize& a, const QSize& b) { return a.width() < b.width(); });
                    icon.pixmap(maxSize).save(iconDir() + "/orig_" + fileName, "PNG");
                    qDebug().nospace() << "[DiskCache::savePwa] saved orig size " << maxSize << " as orig_" << fileName;
                }
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

        QPixmap sourcePix;
        if (auto uwpDir = getUwpInstallDirFromHwnd(hwnd); !uwpDir.isEmpty()) {
            QIcon uwpIcon = AppUtil::getAppIcon(uwpDir + "\\fake.exe");
            sourcePix = uwpIcon.pixmap(256);
#ifndef NDEBUG
            if (path.contains("ApplicationFrameHost", Qt::CaseInsensitive))
                qDebug().nospace() << "[getCachedIcon] AppFrameHost UWP hit: uwpDir=" << uwpDir
                                   << " uwpIconSize=" << sourcePix.size();
#endif
        } else {
#ifndef NDEBUG
            if (path.contains("ApplicationFrameHost", Qt::CaseInsensitive))
                qDebug().nospace() << "[getCachedIcon] AppFrameHost UWP miss -> fallback to extractJumboIconPixmap";
#endif
            sourcePix = extractJumboIconPixmap(path);
        }

        QIcon icon(sourcePix);
        DiskCache::instance().saveExeIcon(path, sourcePix);
        IconCache.insert(path, icon);
        qDebug() << "Icon not found in cache, loaded in" << t.elapsed() << "ms" << path << "source=" << sourcePix.size();
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
                // Read GDI raw data as straight alpha (Format_ARGB32), NOT premultiplied.
                // GDI's GetDIBits returns straight alpha BGRA pixels.
                // Qt's QImage::Format_ARGB32_Premultiplied would misinterpret them.
                img = QImage(bm.bmWidth, bm.bmHeight, QImage::Format_ARGB32);
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

                // Convert straight alpha to premultiplied alpha via Qt's correct conversion
                img = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
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

    // ── tryGetWindowIcon ──
    // WM_GETICON 只保留两级：ICON_BIG（窗口自定图标） ->  GCLP_HICON（窗口类默认图标）
    // 移除 ICON_SMALL2 / ICON_SMALL / GCLP_HICONSM，避免小尺寸挡住高质量路径
    static QIcon tryGetWindowIcon(HWND hwnd) {
        auto fromHicn = [](HICON hico, bool owned) -> QIcon {
            if (!hico) return {};
            QIcon icon(QPixmap::fromImage(QImage::fromHICON(hico)));
            if (owned) DestroyIcon(hico);
            return icon;
        };

        DWORD_PTR result = 0;
        if (SendMessageTimeoutW(hwnd, WM_GETICON, ICON_BIG, 0,
                                SMTO_ABORTIFHUNG, 1000, &result) && result)
            return fromHicn((HICON)result, true);

        if (HICON hico = (HICON)GetClassLongPtrW(hwnd, GCLP_HICON))
            return fromHicn(hico, false);

        return {};
    }

    QPixmap getFileIcon(const QString& filePath, int size) {
        if (filePath.isEmpty()) return {};

        CComPtr<IShellItem> shellItem;
        HRESULT hr = SHCreateItemFromParsingName(
            reinterpret_cast<const wchar_t*>(filePath.utf16()),
            nullptr,
            IID_PPV_ARGS(&shellItem));
        if (FAILED(hr) || !shellItem) return {};

        CComPtr<IShellItemImageFactory> imgFactory;
        hr = shellItem->QueryInterface(IID_PPV_ARGS(&imgFactory));
        if (FAILED(hr) || !imgFactory) return {};

        HBITMAP hBitmap = nullptr;
        SIZE sz = {size, size};
        hr = imgFactory->GetImage(sz, SIIGBF_BIGGERSIZEOK, &hBitmap);
        if (FAILED(hr) || !hBitmap) return {};

        QImage img;
        {
            BITMAP bm = {};
            GetObject(hBitmap, sizeof(bm), &bm);
            if (bm.bmBitsPixel == 32) {
                img = QImage(bm.bmWidth, bm.bmHeight, QImage::Format_ARGB32);
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
                img = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
            } else {
                img = QImage::fromHBITMAP(hBitmap);
            }
        }
        DeleteObject(hBitmap);
        return QPixmap::fromImage(img);
    }

    QPixmap getIconFromAumid(const QString& aumid) {
        if (aumid.isEmpty()) return {};

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
        SIZE sz = {256, 256};
        hr = imgFactory->GetImage(sz, SIIGBF_BIGGERSIZEOK, &hBitmap);
        if (FAILED(hr) || !hBitmap) return {};

        QImage img;
        {
            BITMAP bm = {};
            GetObject(hBitmap, sizeof(bm), &bm);
            if (bm.bmBitsPixel == 32) {
                img = QImage(bm.bmWidth, bm.bmHeight, QImage::Format_ARGB32);
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
                img = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
            } else {
                img = QImage::fromHBITMAP(hBitmap);
            }
        }
        DeleteObject(hBitmap);
        return QPixmap::fromImage(img);
    }

    // ── resolveWindowIcon ──
    // 按应用类型分场景选择图标来源。各场景内部按优先级逐级 fallback，
    // 每个 fallback 步都记录 source tag + source size 用于调试。
    //
    IconResult resolveWindowIcon(HWND hwnd, const QString& processPath,
                                 const QString& appUserModelId,
                                 WindowKind windowKind,
                                 const QString& processName,
                                 const AppIdentity& identity) {
        static QHash<QString, IconResult> s_cache;

        QString cacheKey;
        if (windowKind == WindowKind::Pwa && !appUserModelId.isEmpty())
            cacheKey = QStringLiteral("PWA:") + appUserModelId;
        else if (!appUserModelId.isEmpty())
            cacheKey = QStringLiteral("AUMID:") + appUserModelId;
        else if (!identity.instance.isEmpty())
            cacheKey = QStringLiteral("INST:") + identity.groupKey();
        else
            cacheKey = QStringLiteral("EXE:") + processPath;

        if (auto it = s_cache.constFind(cacheKey); it != s_cache.constEnd())
            return it.value();

        auto bestSize = [](const QIcon& ic) -> QSize {
            auto sizes = ic.availableSizes();
            if (sizes.isEmpty()) return {};
            return *std::max_element(sizes.begin(), sizes.end(),
                [](const QSize& a, const QSize& b) {
                    return a.width() * a.height() < b.width() * b.height();
                });
        };

        IconResult result;

        // ── Scene 1: PWA ──
        if (windowKind == WindowKind::Pwa && !appUserModelId.isEmpty()) {
            result.icon = getCachedPwaIcon(appUserModelId);
            if (!result.icon.isNull()) {
                result.sourceSize = bestSize(result.icon);
                result.source = IconSource::ShellJumbo;
            }
            if (result.icon.isNull()) {
                auto pix = getShellAppIcon(hwnd);
                if (!pix.isNull()) {
                    result.icon = QIcon(pix);
                    result.sourceSize = pix.size();
                    result.source = IconSource::Aumid;
                    cachePwaIcon(appUserModelId, result.icon);
                }
            }
            if (result.icon.isNull()) {
                result.icon = tryGetWindowIcon(hwnd);
                if (!result.icon.isNull()) {
                    result.sourceSize = bestSize(result.icon);
                    result.source = IconSource::Window;
                }
            }
            if (result.icon.isNull()) {
                result.icon = getCachedIcon(processPath, hwnd);
                if (!result.icon.isNull()) {
                    result.sourceSize = bestSize(result.icon);
                    result.source = IconSource::ShellJumbo;
                }
            }
        }

        // ── Scene 2: AUMID (Control Panel, Settings 等) ──
        // 如果 AUMID 返回 < 48，说明 shell 只能提供低质量图标，退到 exe 源
        else if (!appUserModelId.isEmpty()) {
            auto pix = getIconFromAumid(appUserModelId);
            if (!pix.isNull() && pix.width() >= 48 && pix.height() >= 48) {
                result.icon = QIcon(pix);
                result.sourceSize = pix.size();
                result.source = IconSource::Aumid;
            }
            if (result.icon.isNull()) {
                result.icon = tryGetWindowIcon(hwnd);
                if (!result.icon.isNull()) {
                    result.sourceSize = bestSize(result.icon);
                    result.source = IconSource::Window;
                }
            }
            if (result.icon.isNull()) {
                result.icon = getCachedIcon(processPath, hwnd);
                if (!result.icon.isNull()) {
                    result.sourceSize = bestSize(result.icon);
                    result.source = IconSource::ShellJumbo;
                }
            }
        }

        // ── Scene 3: MMC ──
        else if (processName == QStringLiteral("mmc.exe")
                 && !identity.instance.isEmpty()) {
            auto pix = getFileIcon(identity.instance);
            if (!pix.isNull() && pix.width() >= 48 && pix.height() >= 48) {
                result.icon = QIcon(pix);
                result.sourceSize = pix.size();
                result.source = IconSource::Aumid;
            }
            if (result.icon.isNull()) {
                result.icon = getCachedIcon(processPath, hwnd);
                if (!result.icon.isNull()) {
                    result.sourceSize = bestSize(result.icon);
                    result.source = IconSource::ShellJumbo;
                }
            }
            if (result.icon.isNull()) {
                result.icon = tryGetWindowIcon(hwnd);
                if (!result.icon.isNull()) {
                    result.sourceSize = bestSize(result.icon);
                    result.source = IconSource::Window;
                }
            }
        }

        // ── Scene 4: 普通 Win32 ──
        else {
            result.icon = getCachedIcon(processPath, hwnd);
            if (!result.icon.isNull()) {
                result.sourceSize = bestSize(result.icon);
                result.source = IconSource::ShellJumbo;
            }
            if (result.icon.isNull()) {
                result.icon = tryGetWindowIcon(hwnd);
                if (!result.icon.isNull()) {
                    result.sourceSize = bestSize(result.icon);
                    result.source = IconSource::Window;
                }
            }
            if (result.icon.isNull()) {
                QPixmap pix = QFileIconProvider().icon(QFileInfo(processPath)).pixmap(48);
                if (!pix.isNull()) {
                    result.icon = QIcon(pix);
                    result.sourceSize = pix.size();
                    result.source = IconSource::Provider;
                }
            }
        }

        if (!result.icon.isNull()) {
            static const char* tag[] = {"None","Aumid","Jumbo","Extract","Window","Provider"};
            qDebug().noquote() << "[IconUtil] ICON" << cacheKey
                               << "src=" << tag[static_cast<int>(result.source)]
                               << "native=" << result.sourceSize;
            s_cache.insert(cacheKey, result);
        }
        return result;
    }

    IconResult resolveWindowIcon(const WindowDescriptor& desc) {
        return resolveWindowIcon(desc.hwnd, desc.processPath,
                                 desc.appUserModelId, desc.windowKind,
                                 desc.processName, desc.identity);
    }

    // ── resolveDisplayName ──
    // 独立于 identity 和 icon，只负责"用户看到什么名称"
    QString resolveDisplayName(const QString& pwaDisplayName,
                               const AppIdentity& identity,
                               const QString& title,
                               const QString& processPath,
                               WindowKind windowKind) {
        if (windowKind == WindowKind::Pwa && !pwaDisplayName.isEmpty()) {
            qDebug().noquote() << "[IconUtil] resolveDisplayName path=PWA displayName  "
                               << pwaDisplayName;
            return pwaDisplayName;
        }

        if (!identity.instance.isEmpty()) {
            qDebug().noquote() << "[IconUtil] resolveDisplayName path=instance title   "
                               << title;
            return title;
        }

        QString fileDesc = Util::getFileDescription(processPath);
        if (!fileDesc.isEmpty()) {
            qDebug().noquote() << "[IconUtil] resolveDisplayName path=fileDescription  "
                               << fileDesc;
            return fileDesc;
        }

        return QFileInfo(processPath).fileName();
    }

    QString resolveDisplayName(const WindowDescriptor& desc) {
        return resolveDisplayName(desc.pwaDisplayName, desc.identity,
                                  desc.title, desc.processPath, desc.windowKind);
    }

    QIcon overlayIcon(const QPixmap& icon, const QPixmap& overlay, const QRect& overlayRect) {
        QPixmap bg = icon;
        QPainter painter(&bg);
        painter.drawPixmap(overlayRect, overlay);
        painter.end();
        return bg;
    }
}
