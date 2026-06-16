#ifndef WIN_SWITCHER_SETTINGSSTYLEHELPER_H
#define WIN_SWITCHER_SETTINGSSTYLEHELPER_H

#include <QDialog>
#include <QList>

class QAbstractButton;

namespace Ui { class SettingsDialog; }

namespace SettingsStyleHelper {

void applyTheme(QDialog* dialog, Ui::SettingsDialog* ui,
                const QList<QAbstractButton*>& extraButtons = {});

} // namespace SettingsStyleHelper

#endif //WIN_SWITCHER_SETTINGSSTYLEHELPER_H
