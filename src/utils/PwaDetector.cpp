#include "utils/PwaDetector.h"
#include "lifecycle/IconUtil.h"
#include "core/ConfigManager.h"
#include <QFileInfo>
#include <QFileInfoList>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QCryptographicHash>
#include <propsys.h>
#include <propkey.h>

namespace PwaDetector {

    QString getAppUserModelId(HWND hwnd) {
        if (!hwnd) return {};

        IPropertyStore* store = nullptr;
        HRESULT hr = SHGetPropertyStoreForWindow(hwnd, IID_PPV_ARGS(&store));
        if (FAILED(hr) || !store) return {};

        PROPVARIANT pv;
        PropVariantInit(&pv);

        QString aumid;
        hr = store->GetValue(PKEY_AppUserModel_ID, &pv);
        if (SUCCEEDED(hr) && pv.vt == VT_LPWSTR && pv.pwszVal) {
            aumid = QString::fromWCharArray(pv.pwszVal);
            qDebug().noquote() << "[PwaDetector] AUMID =" << aumid << "hwnd =" << hwnd;
        }

        PropVariantClear(&pv);
        store->Release();
        return aumid;
    }

    bool isPwaWindow(const QString& processPath, const QString& appUserModelId) {
        if (appUserModelId.isEmpty()) return false;

        QString processName = QFileInfo(processPath).fileName().toLower();
        static const QStringList pwaBrowsers = {
            "chrome.exe", "msedge.exe", "chromium.exe"
        };
        if (pwaBrowsers.contains(processName))
            return appUserModelId.contains("_crx_");
        return false;
    }

    // Chromium AUMID rule: extension_id (32) → AUMID suffix (25) = left(10) + mid(17)
    static QString makeAumidKey(const QString& extensionId) {
        if (extensionId.size() != 32)
            return {};
        return extensionId.left(10) + extensionId.mid(16);
    }

    static QStringList getManifestResourcesDirs() {
        QString localAppData = QStandardPaths::writableLocation(
            QStandardPaths::GenericDataLocation);
        return {
            localAppData + "/Chromium/User Data/Default/Web Applications/Manifest Resources",
            localAppData + "/Google/Chrome/User Data/Default/Web Applications/Manifest Resources",
            localAppData + "/Microsoft/Edge/User Data/Default/Web Applications/Manifest Resources",
        };
    }

    // Build index: AUMID suffix → full path to extension directory.
    // Scans all known browser paths once and caches the result.
    static const QHash<QString, QString>& getDirectoryIndex() {
        static QHash<QString, QString> index;
        static bool built = false;
        if (built) return index;
        built = true;

        for (const auto& baseDir : getManifestResourcesDirs()) {
            QDir dir(baseDir);
            if (!dir.exists()) {
                qDebug().noquote() << "[PwaDetector] scan dir NOT FOUND:" << baseDir;
                continue;
            }
            qDebug().noquote() << "[PwaDetector] scanning" << baseDir;
            auto entries = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::NoSort);
            for (const auto& entry : entries) {
                QString key = makeAumidKey(entry);
                if (key.isEmpty()) continue;
                QString fullPath = dir.absoluteFilePath(entry);
                if (index.contains(key)) {
                    qWarning() << "[PwaDetector] Duplicate PWA key:" << key;
                } else {
                    index.insert(key, fullPath);
                    qDebug().noquote() << "[PwaDetector]   indexed" << entry << "->" << key;
                }
            }
        }
        qDebug().noquote() << "[PwaDetector] indexed" << index.size() << "PWA directories total";
        return index;
    }

    // Try loading the largest PNG (>=64) from the Manifest Resources directory.
    static QIcon loadPwaIconFromDisk(const QString& appUserModelId) {
        if (appUserModelId.isEmpty())
            return {};
        int idx = appUserModelId.indexOf("_crx_");
        if (idx < 0) return {};
        QString aumidSuffix = appUserModelId.mid(idx + 5);

        qDebug().noquote() << "[PwaDetector] lookup aumidSuffix =" << aumidSuffix;

        const auto& dirIndex = getDirectoryIndex();
        auto it = dirIndex.find(aumidSuffix);
        if (it == dirIndex.end()) {
            qDebug().noquote() << "[PwaDetector]   MISS - not in index (indexSize="
                     << dirIndex.size() << ")";
            return {};
        }

        qDebug().noquote() << "[PwaDetector]   HIT ->" << it.value();

        QDir iconsDir(it.value() + "/Icons");
        if (!iconsDir.exists()) {
            qDebug().noquote() << "[PwaDetector]   icons dir NOT FOUND:"
                     << iconsDir.path();
            return {};
        }

        auto files = iconsDir.entryInfoList({"*.png"}, QDir::Files, QDir::NoSort);
        qDebug().noquote() << "[PwaDetector]   found" << files.size()
                 << "PNGs in" << iconsDir.path();

        QFileInfo best;
        int bestDim = 0;
        for (const auto& fi : files) {
            bool ok;
            int dim = fi.baseName().toInt(&ok);
            qDebug().noquote() << "[PwaDetector]     candidate:" << fi.fileName()
                     << "dim=" << dim;
            if (ok && dim >= 64 && dim > bestDim) {
                bestDim = dim;
                best = fi;
            }
        }
        if (!best.exists()) {
            qDebug().noquote() << "[PwaDetector]   no icon >= 64 in"
                     << iconsDir.path();
            return {};
        }

        QIcon icon(best.absoluteFilePath());
        if (icon.isNull()) {
            qDebug().noquote() << "[PwaDetector]   QIcon null for"
                     << best.absoluteFilePath();
            return {};
        }

        QPixmap pm = icon.pixmap(icon.availableSizes().value(0, QSize(0,0)));
        qDebug().noquote() << "[PwaDetector] manifest icon:"
                 << best.fileName() << best.absoluteFilePath()
                 << pm.size() << "aumid =" << appUserModelId;
        return icon;
    }

    QIcon getPwaIcon(HWND hwnd, const QString& appUserModelId, const QString& fallbackExePath) {
        // UI thread only
        static QHash<QString, QIcon> memCache;

        QString cachePath;
        if (!appUserModelId.isEmpty()) {
            if (auto icon = memCache.value(appUserModelId); !icon.isNull())
                return icon;

            // Try Manifest Resources on disk FIRST (Chromium/Chrome/Edge PWAs)
            QIcon manifestIcon = loadPwaIconFromDisk(appUserModelId);
            if (!manifestIcon.isNull()) {
                memCache.insert(appUserModelId, manifestIcon);
                QDir().mkpath(cfg().getIconCacheDirectory() + "/icons");
                cachePath = cfg().getIconCacheDirectory() + "/icons/pwa_"
                    + QCryptographicHash::hash(
                        appUserModelId.toUtf8(), QCryptographicHash::Md5).toHex()
                    + ".png";
                auto sizes = manifestIcon.availableSizes();
                if (!sizes.isEmpty())
                    manifestIcon.pixmap(sizes.first()).save(cachePath, "PNG");
                return manifestIcon;
            }

            // Fall back to old disk cache (may contain old low-res icon)
            cachePath = cfg().getIconCacheDirectory() + "/icons/pwa_"
                + QCryptographicHash::hash(
                    appUserModelId.toUtf8(), QCryptographicHash::Md5).toHex()
                + ".png";
            if (QFile::exists(cachePath)) {
                QIcon icon(cachePath);
                if (!icon.isNull()) {
                    memCache.insert(appUserModelId, icon);
                    return icon;
                }
            }
        }

        qDebug().noquote() << "[PwaDetector] manifest miss, fallback to window icon";
        QPixmap pix = Util::getWindowIcon(hwnd);
        if (!pix.isNull()) {
            qDebug().noquote() << "[PwaDetector] window icon size =" << pix.size()
                     << "aumid =" << appUserModelId;
            QIcon icon(pix);
            if (!appUserModelId.isEmpty()) {
                memCache.insert(appUserModelId, icon);
                QDir().mkpath(cfg().getIconCacheDirectory() + "/icons");
                icon.pixmap(pix.size()).save(cachePath, "PNG");
            }
            return icon;
        }

        return Util::getCachedIcon(fallbackExePath, hwnd);
    }

}
