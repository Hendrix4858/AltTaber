#ifndef WIN_SWITCHER_SETTINGSSTYLEHELPER_H
#define WIN_SWITCHER_SETTINGSSTYLEHELPER_H

#include <QDialog>

namespace Ui { class SettingsDialog; }

namespace SettingsStyleHelper {

void applyTheme(QDialog* dialog, Ui::SettingsDialog* ui);

} // namespace SettingsStyleHelper

#endif //WIN_SWITCHER_SETTINGSSTYLEHELPER_H
