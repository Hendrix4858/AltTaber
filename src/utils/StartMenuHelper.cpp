#include "utils/StartMenuHelper.h"
#include "utils/UwpHelper.h"
#include <QDebug>
#include <KnownFolders.h>
#include <ShlGuid.h>
#include <ShObjIdl_core.h>
#include <propkey.h>
#include <atlbase.h>

namespace AppUtil {
    QList<std::tuple<QString, QString, QString>> getStartAppList() {
        IShellItem* psi = nullptr;
        HRESULT hr = SHCreateItemInKnownFolder(FOLDERID_AppsFolder, 0, nullptr, IID_PPV_ARGS(&psi));

        QList<std::tuple<QString, QString, QString>> appList;

        if (SUCCEEDED(hr)) {
            IEnumShellItems* pEnum = nullptr;
            hr = psi->BindToHandler(nullptr, BHID_EnumItems, IID_PPV_ARGS(&pEnum));

            if (SUCCEEDED(hr)) {
                IShellItem* pChildItem = nullptr;
                while (pEnum->Next(1, &pChildItem, nullptr) == S_OK) {
                    PWSTR _displayName = nullptr;
                    PWSTR _relativePath = nullptr;
                    auto hr_name = pChildItem->GetDisplayName(SIGDN_NORMALDISPLAY, &_displayName);
                    auto hr_path = pChildItem->GetDisplayName(SIGDN_PARENTRELATIVEPARSING, &_relativePath);

                    QString name, relPath;
                    if (SUCCEEDED(hr_path) && SUCCEEDED(hr_name)) {
                        name = QString::fromWCharArray(_displayName);
                        relPath = QString::fromWCharArray(_relativePath);
                    } else
                        qWarning() << "Failed to get display name or relPath.";

                    CComPtr<IPropertyStore> store;
                    hr = pChildItem->BindToHandler(nullptr, BHID_PropertyStore, IID_PPV_ARGS(&store));
                    QString exePath;
                    if (SUCCEEDED(hr)) {
                        PROPVARIANT var;
                        PropVariantInit(&var);
                        hr = store->GetValue(PKEY_Link_TargetParsingPath, &var);
                        if (SUCCEEDED(hr) && var.vt == VT_LPWSTR) {
                            exePath = QString::fromWCharArray(var.pwszVal);
                        } else {
                            exePath = AppUtil::getUwpExePathByAUMID(relPath);
                        }
                        PropVariantClear(&var);
                    } else
                        qWarning() << "Failed to get property store.";

                    appList.append({name, relPath, exePath});

                    CoTaskMemFree(_relativePath);
                    CoTaskMemFree(_displayName);
                    pChildItem->Release();
                }
                pEnum->Release();
            }
            psi->Release();
        }

        return appList;
    }
}
