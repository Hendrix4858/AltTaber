#include "utils/PathUtils.h"
#include <QApplication>
#include <QDir>

namespace PathUtils {

QString resolveAppRelativePath(const QString& path, const QString& defaultSuffix) {
    if (path.isEmpty())
        return QApplication::applicationDirPath() + "/" + defaultSuffix;
    if (QDir::isRelativePath(path))
        return QApplication::applicationDirPath() + "/" + path;
    return path;
}

} // namespace PathUtils
