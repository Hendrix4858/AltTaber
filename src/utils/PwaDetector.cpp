#include "utils/PwaDetector.h"
#include "lifecycle/IconUtil.h"
#include <QFileInfo>
#include <QDebug>
#include <propsys.h>
#include <propkey.h>
#include <shobjidl.h>

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

    QIcon getPwaIcon(HWND hwnd, const QString& appUserModelId, const QString& fallbackExePath) {
        if (!appUserModelId.isEmpty()) {
            if (auto icon = Util::getCachedPwaIcon(appUserModelId); !icon.isNull())
                return icon;

            {
                QPixmap shellPix = Util::getShellAppIcon(hwnd);
                if (!shellPix.isNull()) {
                    QIcon icon(shellPix);
                    Util::cachePwaIcon(appUserModelId, icon);
                    return icon;
                }
            }

        }

        if (detectPwaType(fallbackExePath, appUserModelId) == PwaType::WindowsAppModel)
            return Util::getCachedIcon(fallbackExePath, hwnd);

        QPixmap pix = Util::getWindowIcon(hwnd);
        if (!pix.isNull()) {
            QIcon icon(pix);
            if (!appUserModelId.isEmpty())
                Util::cachePwaIcon(appUserModelId, icon);
            return icon;
        }

        return Util::getCachedIcon(fallbackExePath, hwnd);
    }

    QString getPwaDisplayName(const QString& appUserModelId) {
        if (appUserModelId.isEmpty())
            return {};

        // Resolve display name from Windows Shell AppsFolder
        QString path = QStringLiteral("shell:AppsFolder\\") + appUserModelId;
        IShellItem* pItem = nullptr;
        HRESULT hr = SHCreateItemFromParsingName(
            reinterpret_cast<LPCWSTR>(path.utf16()), nullptr,
            IID_PPV_ARGS(&pItem));
        if (SUCCEEDED(hr) && pItem) {
            LPWSTR name = nullptr;
            hr = pItem->GetDisplayName(SIGDN_NORMALDISPLAY, &name);
            if (SUCCEEDED(hr) && name) {
                QString result = QString::fromWCharArray(name);
                CoTaskMemFree(name);
                pItem->Release();
                return result;
            }
            pItem->Release();
        }

        // Fallback: extract readable name from AUMID (WindowsAppModel type)
        int bangPos = appUserModelId.lastIndexOf('!');
        if (bangPos > 0) {
            QString appPart = appUserModelId.left(bangPos);
            int dotPos = appPart.lastIndexOf('.');
            if (dotPos > 0)
                return appPart.mid(dotPos + 1);
            return appPart;
        }

        return {};
    }

}
