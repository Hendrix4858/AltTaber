#ifndef WIN_SWITCHER_APPUTIL_H
#define WIN_SWITCHER_APPUTIL_H

#include "utils/UwpHelper.h"
#include "utils/StartMenuHelper.h"

namespace AppUtil {
    QString getExePathFromAppIdOrName(const QString& appid = QString(), const QString& appName = QString());
}

#endif //WIN_SWITCHER_APPUTIL_H
