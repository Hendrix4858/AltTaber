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
#include <QRegularExpression>
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

    PwaType detectPwaType(const QString& processPath, const QString& appUserModelId) {
        if (appUserModelId.isEmpty()) return PwaType::None;

        QString processName = QFileInfo(processPath).fileName().toLower();
        bool knownBrowser = processName == "chrome.exe" || processName == "msedge.exe"
                         || processName == "chromium.exe";
        if (!knownBrowser) return PwaType::None;

        if (appUserModelId.contains("_crx_"))
            return PwaType::ChromiumCrx;

        // Edge Windows AppModel PWA: gemini.google.com-D0A8E439_vn3jms8s81tkg!App
        if (processName == "msedge.exe"
            && appUserModelId.endsWith("!App")
            && appUserModelId.contains('.'))
        {
            return PwaType::WindowsAppModel;
        }

        return PwaType::None;
    }

    bool isPwaWindow(const QString& processPath, const QString& appUserModelId) {
        return detectPwaType(processPath, appUserModelId) != PwaType::None;
    }

    static QString extractCrxId(const QString& aumid) {
        static const QRegularExpression re(
            R"(_crx_+([a-z0-9]+))",
            QRegularExpression::CaseInsensitiveOption);
        auto match = re.match(aumid);
        if (!match.hasMatch())
            return {};
        return match.captured(1);
    }

    static bool isValidCrxId(const QString& id) {
        return id.length() >= 16;
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
    //
    // Windows AUMID has a 64-character limit (kMaxAppModelIdLength in
    // chrome/installer/util/util_constants.h). The PWA component is
    // "_crx_" + 32-char app_id = 37 chars, exceeding the per-component
    // max of 31 (64 / 2 components - 1 separator). Chromium shortens it
    // via ShortenAppModelIdComponent() in shell_util.cc which keeps the
    // outer portions: left(15) + right(16). After stripping the "_crx_"
    // prefix, the Chrome AUMID suffix becomes left(10) + right(16) = 26
    // chars. Edge's prefix is 1 char longer, yielding left(9)+right(16)
    // = 25 chars. We insert both keys to cover both browsers.
    static const QHash<QString, QString>& getDirectoryIndex() {
        static QHash<QString, QString> index;
        static bool built = false;
        if (built) return index;
        built = true;

        for (const auto& baseDir : getManifestResourcesDirs()) {
            QDir dir(baseDir);
            if (!dir.exists()) continue;
            auto entries = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::NoSort);
            for (const auto& entry : entries) {
                QString fullPath = dir.absoluteFilePath(entry);
                index.insert(entry, fullPath);
                index.insert(entry.left(10) + entry.right(16), fullPath);
                index.insert(entry.left(9)  + entry.right(16), fullPath);
            }
        }
        return index;
    }

    // Try loading the largest PNG (>=64) from the Manifest Resources directory.
    static QIcon loadPwaIconFromDisk(const QString& appUserModelId) {
        if (appUserModelId.isEmpty() || !appUserModelId.contains("_crx_"))
            return {};

        QString crxId = extractCrxId(appUserModelId);
        if (crxId.isEmpty() || !isValidCrxId(crxId))
            return {};

        const auto& dirIndex = getDirectoryIndex();
        auto it = dirIndex.find(crxId);
        if (it == dirIndex.end())
            return {};

        QDir iconsDir(it.value() + "/Icons");
        if (!iconsDir.exists())
            return {};

        auto files = iconsDir.entryInfoList({"*.png"}, QDir::Files, QDir::NoSort);
        QFileInfo best;
        int bestDim = 0;
        for (const auto& fi : files) {
            bool ok;
            int dim = fi.baseName().toInt(&ok);
            if (ok && dim >= 64 && dim > bestDim) {
                bestDim = dim;
                best = fi;
            }
        }
        if (!best.exists())
            return {};

        QIcon icon(best.absoluteFilePath());
        if (icon.isNull())
            return {};

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

        QPixmap pix = Util::getWindowIcon(hwnd);
        if (!pix.isNull()) {
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
