#include "utils/MiscUtil.h"
#include <QDebug>
#include <QFileInfo>
#include <ShObjIdl_core.h>
#include <propkey.h>
#include <atlbase.h>
#include <dwmapi.h>

namespace Util {
    QString getWindowTitle(HWND hwnd) {
        const int len = GetWindowTextLength(hwnd) + 1;
        if (len <= 1) return {};
        auto* title = new wchar_t[len];
        GetWindowText(hwnd, title, len);
        QString result = QString::fromWCharArray(title);
        delete[] title;
        return result;
    }

    QString getClassName(HWND hwnd) {
        const int MAX = 256;
        wchar_t className[MAX];
        GetClassName(hwnd, className, MAX);
        return QString::fromWCharArray(className);
    }

    bool isWindowCloaked(HWND hwnd) {
        BOOL isCloaked = false;
        auto rt = DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &isCloaked, sizeof(isCloaked));
        return rt == S_OK && isCloaked;
    }

    bool isTopMost(HWND hwnd) {
        return GetWindowLong(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST;
    }

    QString getFileDescription(const QString& path) {
        QString desc = QFileInfo(path).completeBaseName();
        CComPtr<IShellItem2> pItem;
        HRESULT hr = SHCreateItemFromParsingName(path.toStdWString().c_str(), nullptr, IID_PPV_ARGS(&pItem));
        if (SUCCEEDED(hr)) {
            CComHeapPtr<WCHAR> pValue;
            hr = pItem->GetString(PKEY_FileDescription, &pValue);
            if (SUCCEEDED(hr))
                desc = QString::fromWCharArray(pValue);
            else
                qWarning() << "No FileDescription, fallback to file name:" << desc;
        }
        return desc;
    }

#include "utils/PinyinTable.inc"

    QChar getDisplayFirstLetter(const QString& text) {
        if (text.isEmpty()) return {};
        auto first = text[0];
        auto uc = first.unicode();

        if (uc >= 'A' && uc <= 'Z') return first;
        if (uc >= 'a' && uc <= 'z') return QChar(uc - 32);

        if (uc >= 0x4E00 && uc <= 0x9FFF)
            return getPinyinInitial(uc);

        return {};
    }

    bool isKeyPressed(int vkey) {
        return GetAsyncKeyState(vkey) & 0x8000;
    }

    bool setWindowRoundCorner(HWND hwnd, DWM_WINDOW_CORNER_PREFERENCE pvAttribute) {
        HRESULT hr = DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &pvAttribute, sizeof(pvAttribute));
        if (FAILED(hr)) {
            qWarning() << "Failed to set rounded corners for window:" << hr;
            return false;
        }
        return true;
    }

    POINT getCursorPos() {
        POINT pos;
        GetCursorPos(&pos);
        return pos;
    }
}
