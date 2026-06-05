#include "utils/AppUtil.h"
#include "utils/UwpHelper.h"
#include "utils/StartMenuHelper.h"
#include "utils/Util.h"
#include <QDebug>
#include <QFile>
#include <QFileInfo>

namespace AppUtil {
    QString getExePathFromAppIdOrName(const QString& appid, const QString& appName) {
        static QHash<QString, QString> app2Path;
        static QHash<QString, QString> desc2Path;
        static auto buildStartAppMaps = []() {
            app2Path.clear();
            const auto list = getStartAppList();
            for (const auto& [name, appId, exePath]: list) {
                app2Path.insert(appId, exePath);
                app2Path.insert(name, exePath);
            }
        };
        if (app2Path.isEmpty())
            buildStartAppMaps();

        if (appid.isEmpty()) return {};
        if (QFile::exists(appid))
            return appid;
        if (appid == "Microsoft.Windows.Explorer")
            return "C:\\Windows\\explorer.exe";
        static QStringList BlackList;
        if (BlackList.contains(appid)) return {};

        int retry = 1;
        do {
            if (auto exe = app2Path.value(appid); !exe.isEmpty()) {
                qDebug() << "Get path by AppId:" << appid;
                return exe;
            }
            if (auto exe = app2Path.value(appName); !exe.isEmpty()) {
                qDebug() << "Get path by name:" << appName;
                return exe;
            }
            if (auto exe = desc2Path.value(appName); !exe.isEmpty()) {
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
                desc2Path.insert(description, path);
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
        BlackList.append(appid);
        return {};
    }
}
