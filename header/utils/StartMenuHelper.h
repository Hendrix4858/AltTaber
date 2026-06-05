#ifndef WIN_SWITCHER_STARTMENUHELPER_H
#define WIN_SWITCHER_STARTMENUHELPER_H

#include <QList>
#include <QString>
#include <tuple>

namespace AppUtil {
    QList<std::tuple<QString, QString, QString>> getStartAppList();
}

#endif //WIN_SWITCHER_STARTMENUHELPER_H
