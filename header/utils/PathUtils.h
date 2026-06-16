#ifndef WIN_SWITCHER_PATHUTILS_H
#define WIN_SWITCHER_PATHUTILS_H

#include <QString>

namespace PathUtils {

QString resolveAppRelativePath(const QString& path, const QString& defaultSuffix);

} // namespace PathUtils

#endif //WIN_SWITCHER_PATHUTILS_H
