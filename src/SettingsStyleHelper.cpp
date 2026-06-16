#include "SettingsStyleHelper.h"
#include "ui_SettingsDialog.h"
#include "core/ThemeManager.h"
#include <QDialog>
#include <QAbstractButton>

namespace SettingsStyleHelper {

void applyTheme(QDialog* dialog, Ui::SettingsDialog* ui,
                const QList<QAbstractButton*>& extraButtons) {
    const auto& c = ThemeManager::current();

    dialog->setStyleSheet(QString("QDialog { background-color: %1; }").arg(c.bgColor.name()));

    ui->topBar->setStyleSheet(
        QString("background-color: %1; border-bottom: 1px solid %2;")
            .arg(c.panelColor.name(), c.borderColor.name()));

    const QString lineEditStyle = QString(
        "QLineEdit { padding: 6px 10px; border: 1px solid %1; border-radius: 4px;"
        "  background-color: %2; color: %3; font-size: 13px; }"
        "QLineEdit:focus { border-color: %4; }"
    );

    ui->searchEdit->setStyleSheet(lineEditStyle.arg(
        c.borderColor.name(), c.inputBg.name(), c.textColor.name(), c.accentColor.name()));

    ui->navList->setStyleSheet(QString(
        "QListWidget {"
        "  background-color: %1; color: %2; border-right: 1px solid %3;"
        "  border-top: none; border-left: none; border-bottom: none; outline: none;"
        "  font-size: 13px;"
        "}"
        "QListWidget::item { padding: 10px 16px; border: none; }"
        "QListWidget::item:selected { background-color: %4; color: white; }"
        "QListWidget::item:hover:!selected { background-color: %5; }"
    ).arg(c.panelColor.name(), c.textColor.name(), c.borderColor.name(),
          c.highlightColor.name(), c.delegateHoverUnselected.name()));

    ui->stackedWidget->setStyleSheet(
        QString("background-color: %1;").arg(c.bgColor.name()));

    const QString groupBoxStyle = QString(
        "QGroupBox { color: %1; font-size: 13px; font-weight: bold;"
        "  border: 1px solid %2; border-radius: 6px; margin-top: 12px; padding-top: 16px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; }"
    ).arg(c.textColor.name(), c.borderColor.name());

    ui->langGroup->setStyleSheet(groupBoxStyle);
    ui->letterJumpGroup->setStyleSheet(groupBoxStyle);
    ui->displayGroup->setStyleSheet(groupBoxStyle);
    ui->logGroup->setStyleSheet(groupBoxStyle);
    ui->cacheGroup->setStyleSheet(groupBoxStyle);
    ui->blockedGroup->setStyleSheet(groupBoxStyle);

    const QString comboStyle = QString(
        "QComboBox { padding: 6px 10px; border: 1px solid %1; border-radius: 4px;"
        "  background-color: %2; color: %3; font-size: 13px; }"
        "QComboBox:focus { border-color: %4; }"
        "QComboBox::drop-down { border: none; width: 24px; }"
        "QComboBox::down-arrow { width: 0; }"
        "QComboBox QAbstractItemView { background-color: %2; color: %3;"
        "  border: 1px solid %1; selection-background-color: %5; }"
    ).arg(c.borderColor.name(), c.inputBg.name(), c.textColor.name(),
          c.accentColor.name(), c.highlightColor.name());

    ui->langCombo->setStyleSheet(comboStyle);
    ui->monitorCombo->setStyleSheet(comboStyle);
    ui->themeCombo->setStyleSheet(comboStyle);

    const QString labelStyle =
        QString("color: %1; font-weight: normal;").arg(c.textColor.name());
    ui->langLabel->setStyleSheet(labelStyle);
    ui->monitorLabel->setStyleSheet(labelStyle);
    ui->themeLabel->setStyleSheet(labelStyle);
    ui->minIconSizeLabel->setStyleSheet(labelStyle);
    ui->logDirLabel->setStyleSheet(labelStyle);
    ui->cacheDirLabel->setStyleSheet(labelStyle);

    const QString checkStyle = QString(
        "QCheckBox { color: %1; font-size: 13px; spacing: 8px; margin: 4px 0; }"
        "QCheckBox::indicator { width: 16px; height: 16px; }"
    ).arg(c.textColor.name());

    ui->startupCheck->setStyleSheet(checkStyle);
    ui->adminCheck->setStyleSheet(checkStyle);
    ui->letterJumpCheck->setStyleSheet(checkStyle);
    ui->mouseClickActivateCheck->setStyleSheet(checkStyle);
    ui->clickShowGroupCheck->setStyleSheet(checkStyle);
    ui->iconCacheCheck->setStyleSheet(checkStyle);
    ui->chkLogDebug->setStyleSheet(checkStyle);
    ui->chkLogInfo->setStyleSheet(checkStyle);
    ui->chkLogWarn->setStyleSheet(checkStyle);
    ui->chkLogError->setStyleSheet(checkStyle);
    ui->chkLogFatal->setStyleSheet(checkStyle);

    ui->aboutDesc->setStyleSheet(
        QString("color: %1; font-size: 14px;").arg(c.textColor.name()));

    ui->bottomBar->setStyleSheet(
        QString("background-color: %1; border-top: 1px solid %2;")
            .arg(c.panelColor.name(), c.borderColor.name()));

    const QString btnStyle = QString(
        "QPushButton {"
        "  padding: 6px 20px; border: 1px solid %1; border-radius: 4px;"
        "  background-color: %2; color: %3; font-size: 13px; min-width: 70px;"
        "}"
        "QPushButton:hover { background-color: %4; }"
        "QPushButton:pressed { background-color: %5; }"
        "QPushButton:disabled { color: %6; }"
    ).arg(c.borderColor.name(), c.inputBg.name(), c.textColor.name(),
          c.highlightColor.name(), c.borderColor.name(), c.disabledText.name());

    ui->btnOk->setStyleSheet(btnStyle);
    ui->btnCancel->setStyleSheet(btnStyle);
    ui->btnApply->setStyleSheet(btnStyle);
    ui->btnAddBlocked->setStyleSheet(btnStyle);
    ui->btnRemoveBlocked->setStyleSheet(btnStyle);

    ui->blockedTable->setStyleSheet(QString(
        "QTableWidget { background-color: %1; color: %2; border: 1px solid %3;"
        "  gridline-color: %3; font-size: 13px; }"
        "QTableWidget::item:selected { background-color: %4; color: white; }"
        "QHeaderView::section { background-color: %5; color: %2;"
        "  border: 1px solid %3; padding: 4px; font-weight: bold; }"
    ).arg(c.inputBg.name(), c.textColor.name(), c.borderColor.name(),
          c.highlightColor.name(), c.panelColor.name()));

    ui->logDirEdit->setStyleSheet(lineEditStyle.arg(
        c.borderColor.name(), c.inputBg.name(), c.textColor.name(), c.accentColor.name()));
    ui->cacheDirEdit->setStyleSheet(ui->logDirEdit->styleSheet());

    ui->minIconSizeSpin->setStyleSheet(QString(
        "QSpinBox { padding: 6px 10px; border: 1px solid %1; border-radius: 4px;"
        "  background-color: %2; color: %3; font-size: 13px; }"
        "QSpinBox:focus { border-color: %4; }"
        "QSpinBox::up-button, QSpinBox::down-button {"
        "  border: none; width: 20px; }"
    ).arg(c.borderColor.name(), c.inputBg.name(), c.textColor.name(), c.accentColor.name()));

    for (auto* btn : extraButtons) {
        if (btn)
            btn->setStyleSheet(btnStyle);
    }
}

} // namespace SettingsStyleHelper
