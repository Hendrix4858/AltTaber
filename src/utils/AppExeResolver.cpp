#include "utils/AppUtil.h"
#include "utils/UwpHelper.h"
#include "utils/StartMenuHelper.h"
#include "utils/Util.h"
#include <QDebug>
#include <QFile>
#include <QFileInfo>

namespace AppUtil {
    QString getExePathFromAppIdOrName(const QString& appid, const QString& appName) {
        static QHash<QString, QString> appIdToExePath;
        static QHash<QString, QString> descriptionToExePath;
        static auto buildStartAppMaps = []() {
            appIdToExePath.clear();
            const auto list = getStartAppList();
            for (const auto& [name, appId, exePath]: list) {
                appIdToExePath.insert(appId, exePath);
                appIdToExePath.insert(name, exePath);
            }
        };
        if (appIdToExePath.isEmpty())
            buildStartAppMaps();

        if (appid.isEmpty()) return {};
        if (QFile::exists(appid))
            return appid;
        if (appid == "Microsoft.Windows.Explorer")
            return "C:\\Windows\\explorer.exe";
        static QStringList blacklistedAppIds;
        if (blacklistedAppIds.contains(appid)) return {};

        int retry = 1;
        do {
            if (auto exe = appIdToExePath.value(appid); !exe.isEmpty()) {
                qDebug() << "Get path by AppId:" << appid;
                return exe;
            }
            if (auto exe = appIdToExePath.value(appName); !exe.isEmpty()) {
                qDebug() << "Get path by name:" << appName;
                return exe;
            }
            if (auto exe = descriptionToExePath.value(appName); !exe.isEmpty()) {
                qDebug() << "Get path by description:" << appName;
                return exe;
            }

            qDebug() << "Not Found. Try list windows' description";
            QSet<QString> paths;
            const auto windows = Util::listValidWindows();
            for (auto hwnd: windows) {
                QString path = Util::getWindowProcessPath(hwnd);
                paths.insert(path);
            }
            for (const auto& path: paths) {
                QString description = Util::getFileDescription(path);
                descriptionToExePath.insert(description, path);
                if (description == appName) {
                    qDebug() << "Get path by description_:" << appName;
                    return path;
                }
            }

            if (retry-- <= 0) break;

            buildStartAppMaps();
            qDebug() << "Not Found. Rebuild App2ExePathMap";
        } while (true);

        qWarning() << "Failed to find exe path for appid:" << appid;
        blacklistedAppIds.append(appid);
        return {};
    }
}
