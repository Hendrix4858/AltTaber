#include "SettingsStyleHelper.h"
#include "ui_SettingsDialog.h"
#include "core/ThemeManager.h"
#include <QDialog>

namespace SettingsStyleHelper {

void applyTheme(QDialog* dialog, Ui::SettingsDialog* ui) {
    const auto& c = ThemeManager::current();

    // One single stylesheet on dialog cascades to all children.
    // Type selectors (QLabel, QCheckBox, QPushButton, ...) cover every
    // widget of that type automatically.  #objectName overrides handle
    // special cases (navList, aboutDesc, minIconSizeSpin, …).
    //
    // Benefit: adding a new QLabel / QCheckBox / QPushButton / QLineEdit
    //          to any .ui page just works — no manual registration needed.
    QString sheet = QString(R"(
        QDialog { background-color: %1; }

        /* ---- containers (require #name because QWidget is transparent by default) ---- */
        QWidget#topBar {
            background-color: %2;
            border-bottom: 1px solid %3;
        }
        QWidget#bottomBar {
            background-color: %2;
            border-top: 1px solid %3;
        }
        QStackedWidget#stackedWidget {
            background-color: %1;
        }

        /* ---- group boxes ---- */
        QGroupBox {
            color: %4;
            font-size: 13px;
            font-weight: bold;
            border: 1px solid %3;
            border-radius: 6px;
            margin-top: 12px;
            padding-top: 16px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 12px;
            padding: 0 6px;
        }

        /* ---- labels ---- */
        QLabel {
            color: %4;
            font-weight: normal;
        }
        QLabel#aboutDesc {
            font-size: 14px;
        }

        /* ---- check-boxes / radio-buttons ---- */
        QCheckBox, QRadioButton {
            color: %4;
            font-size: 13px;
            spacing: 8px;
            margin: 4px 0;
        }
        QCheckBox::indicator, QRadioButton::indicator {
            width: 16px;
            height: 16px;
        }

        /* ---- line-edits ---- */
        QLineEdit {
            padding: 6px 10px;
            border: 1px solid %3;
            border-radius: 4px;
            background-color: %5;
            color: %4;
            font-size: 13px;
        }
        QLineEdit:focus {
            border-color: %6;
        }

        /* ---- combo boxes ---- */
        QComboBox {
            padding: 6px 10px;
            border: 1px solid %3;
            border-radius: 4px;
            background-color: %5;
            color: %4;
            font-size: 13px;
        }
        QComboBox:focus {
            border-color: %6;
        }
        QComboBox::drop-down {
            border: none;
            width: 24px;
        }
        QComboBox::down-arrow {
            width: 0;
        }
        QComboBox QAbstractItemView {
            background-color: %5;
            color: %4;
            border: 1px solid %3;
            selection-background-color: %7;
        }

        /* ---- push buttons ---- */
        QPushButton {
            padding: 6px 20px;
            border: 1px solid %3;
            border-radius: 4px;
            background-color: %5;
            color: %4;
            font-size: 13px;
            min-width: 70px;
        }
        QPushButton:hover {
            background-color: %7;
        }
        QPushButton:pressed {
            background-color: %8;
        }
        QPushButton:disabled {
            color: %9;
        }
    )")
        .arg(c.bgColor.name(),           // %1  dialog / stacked bg
             c.panelColor.name(),         // %2  panel bg
             c.borderColor.name(),        // %3  border
             c.textColor.name(),          // %4  text
             c.inputBg.name(),            // %5  input bg
             c.accentColor.name(),        // %6  accent
             c.highlightColor.name(),     // %7  highlight / hover
             c.panelColor.name(),         // %8  pressed
             c.disabledText.name());      // %9  disabled text

    dialog->setStyleSheet(sheet);

    /* ---- special widgets that need fine-grained control ---- */

    // Navigation list (left sidebar) — selected/hover states
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

    // Blocked-windows table (grid lines, header)
    ui->blockedTable->setStyleSheet(QString(
        "QTableWidget { background-color: %1; color: %2; border: 1px solid %3;"
        "  gridline-color: %3; font-size: 13px; }"
        "QTableWidget::item:selected { background-color: %4; color: white; }"
        "QHeaderView::section { background-color: %5; color: %2;"
        "  border: 1px solid %3; padding: 4px; font-weight: bold; }"
    ).arg(c.inputBg.name(), c.textColor.name(), c.borderColor.name(),
          c.highlightColor.name(), c.panelColor.name()));

    // Icon-size spin box (needs up/down arrow overrides)
    ui->minIconSizeSpin->setStyleSheet(QString(
        "QSpinBox { padding: 6px 10px; border: 1px solid %1; border-radius: 4px;"
        "  background-color: %2; color: %3; font-size: 13px; }"
        "QSpinBox:focus { border-color: %4; }"
        "QSpinBox::up-button, QSpinBox::down-button {"
        "  border: none; width: 20px; }"
    ).arg(c.borderColor.name(), c.inputBg.name(), c.textColor.name(), c.accentColor.name()));
}

} // namespace SettingsStyleHelper
