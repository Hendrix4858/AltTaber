#include "utils/PwaDetector.h"
#include "lifecycle/IconUtil.h"
#include "core/ConfigManager.h"
#include <QFileInfo>
#include <QDebug>
#include <QDir>
#include <QFile>
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
        if (processName == "chrome.exe" || processName == "msedge.exe")
            return appUserModelId.contains("_crx_");
        return false;
    }

    QIcon getPwaIcon(HWND hwnd, const QString& appUserModelId, const QString& fallbackExePath) {
        // UI thread only
        static QHash<QString, QIcon> memCache;

        QString cachePath;
        if (!appUserModelId.isEmpty()) {
            if (auto icon = memCache.value(appUserModelId); !icon.isNull())
                return icon;

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
            qDebug().noquote() << "[PwaDetector] icon size =" << pix.size()
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
