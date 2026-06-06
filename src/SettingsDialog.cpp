#include "SettingsDialog.h"
#include "ui_SettingsDialog.h"
#include "utils/ConfigManager.h"
#include "utils/LanguageManager.h"
#include "utils/Startup.h"
#include "utils/ThemeManager.h"

#include <QApplication>
#include <QFont>
#include <QInputDialog>
#include <QMessageBox>

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::SettingsDialog) {
    ui->setupUi(this);

    QFont titleFont = ui->titleLabel->font();
    titleFont.setPointSize(titleFont.pointSize() + 4);
    titleFont.setBold(true);
    ui->titleLabel->setFont(titleFont);

    ui->navList->addItem(tr("General"));
    ui->navList->addItem(tr("Display"));
    ui->navList->addItem(tr("Hotkeys"));
    ui->navList->addItem(tr("Blocked Windows"));
    ui->navList->addItem(tr("About"));

    // theme combo
    ui->themeCombo->addItem(tr("Dark"), Dark);
    ui->themeCombo->addItem(tr("Light"), Light);

    // language combo
    ui->langCombo->addItem("English", "en");
    ui->langCombo->addItem("中文", "zh_CN");

    applyStyleSheet();

    connect(ui->searchEdit, &QLineEdit::textChanged, this, &SettingsDialog::filterPages);
    connect(ui->navList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0 && row < ui->stackedWidget->count())
            ui->stackedWidget->setCurrentIndex(row);
    });
    connect(ui->btnOk, &QPushButton::clicked, this, [this] {
        applySettings();
        accept();
    });
    connect(ui->btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(ui->btnApply, &QPushButton::clicked, this, &SettingsDialog::applySettings);

    connect(ui->mouseClickActivateCheck, &QCheckBox::toggled, this, [this](bool checked) {
        if (!m_loadingSettings)
            ui->clickShowGroupCheck->setEnabled(checked);
    });

    connect(ui->btnAddBlocked, &QPushButton::clicked, this, [this] {
        bool okTitle;
        QString title = QInputDialog::getText(this, tr("Add Blocked Window"),
                                               tr("Window Title (leave empty to match any):"),
                                               QLineEdit::Normal, QString(), &okTitle);
        if (!okTitle) return;

        bool okClass;
        QString className = QInputDialog::getText(this, tr("Add Blocked Window"),
                                                    tr("Class Name (leave empty to match any):"),
                                                    QLineEdit::Normal, QString(), &okClass);
        if (!okClass) return;

        if (title.isEmpty() && className.isEmpty()) {
            QMessageBox::warning(this, tr("Warning"),
                                tr("At least one of Title or Class Name must be filled."));
            return;
        }

        int row = ui->blockedTable->rowCount();
        ui->blockedTable->insertRow(row);
        ui->blockedTable->setItem(row, 0, new QTableWidgetItem(title));
        ui->blockedTable->setItem(row, 1, new QTableWidgetItem(className));
    });

    connect(ui->btnRemoveBlocked, &QPushButton::clicked, this, [this] {
        int row = ui->blockedTable->currentRow();
        if (row >= 0)
            ui->blockedTable->removeRow(row);
    });

    loadSettings();
    ui->navList->setCurrentRow(0);
}

SettingsDialog::~SettingsDialog() {
    delete ui;
}

void SettingsDialog::applyStyleSheet() {
    const auto& c = ThemeManager::current();

    setStyleSheet(QString("QDialog { background-color: %1; }").arg(c.bgColor.name()));

    ui->topBar->setStyleSheet(QString("background-color: %1; border-bottom: 1px solid %2;")
                              .arg(c.panelColor.name(), c.borderColor.name()));

    ui->titleLabel->setStyleSheet(QString("color: %1; background: transparent; border: none;")
                                  .arg(c.textColor.name()));

    ui->searchEdit->setStyleSheet(QString(
        "QLineEdit {"
        "  padding: 6px 10px; border: 1px solid %1; border-radius: 4px;"
        "  background-color: %2; color: %3; font-size: 13px;"
        "}"
        "QLineEdit:focus { border-color: %4; }"
    ).arg(c.borderColor.name(), c.inputBg.name(), c.textColor.name(), c.accentColor.name()));

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

    ui->stackedWidget->setStyleSheet(QString("background-color: %1;").arg(c.bgColor.name()));

    const QString groupBoxStyle = QString(
        "QGroupBox { color: %1; font-size: 13px; font-weight: bold;"
        "  border: 1px solid %2; border-radius: 6px; margin-top: 12px; padding-top: 16px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; }"
    ).arg(c.textColor.name(), c.borderColor.name());
    ui->langGroup->setStyleSheet(groupBoxStyle);
    ui->letterJumpGroup->setStyleSheet(groupBoxStyle);
    ui->displayGroup->setStyleSheet(groupBoxStyle);
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

    ui->langLabel->setStyleSheet(QString("color: %1; font-weight: normal;").arg(c.textColor.name()));
    ui->monitorLabel->setStyleSheet(QString("color: %1; font-weight: normal;").arg(c.textColor.name()));
    ui->themeLabel->setStyleSheet(QString("color: %1; font-weight: normal;").arg(c.textColor.name()));
    ui->minIconSizeLabel->setStyleSheet(QString("color: %1; font-weight: normal;").arg(c.textColor.name()));

    const QString checkStyle = QString(
        "QCheckBox { color: %1; font-size: 13px; spacing: 8px; margin: 4px 0; }"
        "QCheckBox::indicator { width: 16px; height: 16px; }"
    ).arg(c.textColor.name());
    ui->startupCheck->setStyleSheet(checkStyle);
    ui->adminCheck->setStyleSheet(checkStyle);
    ui->letterJumpCheck->setStyleSheet(checkStyle);
    ui->mouseClickActivateCheck->setStyleSheet(checkStyle);
    ui->clickShowGroupCheck->setStyleSheet(checkStyle);

    const QString placeholderStyle = QString("QLabel { color: %1; font-size: 15px; }").arg(c.disabledText.name());
    ui->hotkeyPlaceholder->setStyleSheet(placeholderStyle);

    ui->aboutDesc->setStyleSheet(QString("color: %1; font-size: 14px;").arg(c.textColor.name()));

    ui->bottomBar->setStyleSheet(QString("background-color: %1; border-top: 1px solid %2;")
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

    // Blocked table style
    ui->blockedTable->setStyleSheet(QString(
        "QTableWidget { background-color: %1; color: %2; border: 1px solid %3;"
        "  gridline-color: %3; font-size: 13px; }"
        "QTableWidget::item:selected { background-color: %4; color: white; }"
        "QHeaderView::section { background-color: %5; color: %2;"
        "  border: 1px solid %3; padding: 4px; font-weight: bold; }"
    ).arg(c.inputBg.name(), c.textColor.name(), c.borderColor.name(),
          c.highlightColor.name(), c.panelColor.name()));

    // min icon size spinbox
    ui->minIconSizeSpin->setStyleSheet(QString(
        "QSpinBox { padding: 6px 10px; border: 1px solid %1; border-radius: 4px;"
        "  background-color: %2; color: %3; font-size: 13px; }"
        "QSpinBox:focus { border-color: %4; }"
        "QSpinBox::up-button, QSpinBox::down-button {"
        "  border: none; width: 20px; }"
    ).arg(c.borderColor.name(), c.inputBg.name(), c.textColor.name(), c.accentColor.name()));
}

void SettingsDialog::retranslateUi() {
    setWindowTitle(tr("Settings"));
    ui->titleLabel->setText(tr("Settings"));
    ui->searchEdit->setPlaceholderText(tr("Search..."));

    if (ui->navList->count() >= 5) {
        ui->navList->item(0)->setText(tr("General"));
        ui->navList->item(1)->setText(tr("Display"));
        ui->navList->item(2)->setText(tr("Hotkeys"));
        ui->navList->item(3)->setText(tr("Blocked Windows"));
        ui->navList->item(4)->setText(tr("About"));
    }

    ui->langGroup->setTitle(tr("Language Settings"));
    ui->langLabel->setText(tr("Language:"));

    QString savedLang = ui->langCombo->currentData().toString();
    ui->langCombo->clear();
    ui->langCombo->addItem(tr("English"), "en");
    ui->langCombo->addItem("中文", "zh_CN");
    int idx = ui->langCombo->findData(savedLang);
    if (idx >= 0) ui->langCombo->setCurrentIndex(idx);

    ui->startupCheck->setText(tr("Start with Windows"));
    ui->adminCheck->setText(tr("Always run as administrator"));

    ui->displayGroup->setTitle(tr("Display Settings"));
    ui->monitorLabel->setText(tr("Display Monitor:"));

    int savedMonitor = ui->monitorCombo->currentData().toInt();
    ui->monitorCombo->clear();
    ui->monitorCombo->addItem(tr("Primary Monitor"), PrimaryMonitor);
    ui->monitorCombo->addItem(tr("Mouse Monitor"), MouseMonitor);
    idx = ui->monitorCombo->findData(savedMonitor);
    if (idx >= 0) ui->monitorCombo->setCurrentIndex(idx);

    int savedTheme = ui->themeCombo->currentData().toInt();
    ui->themeCombo->clear();
    ui->themeCombo->addItem(tr("Dark"), Dark);
    ui->themeCombo->addItem(tr("Light"), Light);
    idx = ui->themeCombo->findData(savedTheme);
    if (idx >= 0) ui->themeCombo->setCurrentIndex(idx);

    ui->themeLabel->setText(tr("Theme:"));
    ui->minIconSizeLabel->setText(tr("Min Icon Size:"));

    ui->blockedGroup->setTitle(tr("Blocked Windows"));
    ui->blockedTable->horizontalHeaderItem(0)->setText(tr("Window Title"));
    ui->blockedTable->horizontalHeaderItem(1)->setText(tr("Class Name"));
    ui->btnAddBlocked->setText(tr("Add"));
    ui->btnRemoveBlocked->setText(tr("Remove"));

    ui->letterJumpGroup->setTitle(tr("Letter Jump"));
    ui->letterJumpCheck->setText(tr("Enable letter jump (A-Z)"));
    ui->mouseClickActivateCheck->setText(tr("Activate window on mouse click"));
    ui->clickShowGroupCheck->setText(tr("Show window list for multi-window apps"));
    ui->hotkeyPlaceholder->setText(tr("Hotkey settings coming soon..."));

    QString version = QApplication::applicationVersion();
    ui->aboutDesc->setText(tr("AltTaber - Window Switcher<br>"
                              "Version: %1<br><br>"
                              "A modern Alt+Tab replacement for Windows.<br><br>"
                              "GitHub: <a href='https://github.com/MrBeanCpp/AltTaber' "
                              "style='color: #0078D4;'>MrBeanCpp/AltTaber</a>")
                           .arg(version));

    ui->btnOk->setText(tr("OK"));
    ui->btnCancel->setText(tr("Cancel"));
    ui->btnApply->setText(tr("Apply"));
}

void SettingsDialog::filterPages(const QString& text) {
    for (int i = 0; i < ui->navList->count(); ++i) {
        auto* item = ui->navList->item(i);
        item->setHidden(!text.isEmpty() &&
                        !item->text().contains(text, Qt::CaseInsensitive));
    }

    if (ui->navList->currentItem() && ui->navList->currentItem()->isHidden()) {
        for (int i = 0; i < ui->navList->count(); ++i) {
            if (!ui->navList->item(i)->isHidden()) {
                ui->navList->setCurrentRow(i);
                return;
            }
        }
    }
}

void SettingsDialog::loadSettings() {
    m_loadingSettings = true;

    QString lang = cfg().getLanguage();
    int idx = ui->langCombo->findData(lang);
    if (idx >= 0) ui->langCombo->setCurrentIndex(idx);

    ui->startupCheck->setChecked(Startup::isOn());
    ui->adminCheck->setChecked(cfg().getAlwaysRunAsAdmin());

    idx = ui->monitorCombo->findData(cfg().getDisplayMonitor());
    if (idx >= 0) ui->monitorCombo->setCurrentIndex(idx);

    idx = ui->themeCombo->findData(cfg().getTheme());
    if (idx >= 0) ui->themeCombo->setCurrentIndex(idx);

    ui->minIconSizeSpin->setValue(cfg().getMinIconSize());

    ui->letterJumpCheck->setChecked(cfg().getLetterJumpEnabled());

    ui->mouseClickActivateCheck->setChecked(cfg().getMouseClickActivateEnabled());
    ui->clickShowGroupCheck->setChecked(cfg().getClickShowGroupForMultiWindow());
    ui->clickShowGroupCheck->setEnabled(ui->mouseClickActivateCheck->isChecked());

    // blocked windows
    ui->blockedTable->setRowCount(0);
    auto blocked = cfg().getBlockedWindows();
    for (const auto& entry : blocked) {
        int row = ui->blockedTable->rowCount();
        ui->blockedTable->insertRow(row);
        ui->blockedTable->setItem(row, 0, new QTableWidgetItem(entry.title));
        ui->blockedTable->setItem(row, 1, new QTableWidgetItem(entry.className));
    }

    m_loadingSettings = false;

    retranslateUi();
}

void SettingsDialog::applySettings() {
    QString lang = ui->langCombo->currentData().toString();
    cfg().setLanguage(lang);
    switchLanguage(lang);
    retranslateUi();

    bool startup = ui->startupCheck->isChecked();
    if (startup != Startup::isOn())
        Startup::toggle();

    cfg().setAlwaysRunAsAdmin(ui->adminCheck->isChecked());

    auto monitor = static_cast<DisplayMonitor>(ui->monitorCombo->currentData().toInt());
    cfg().setDisplayMonitor(monitor);

    int theme = ui->themeCombo->currentData().toInt();
    if (theme != cfg().getTheme()) {
        cfg().setTheme(theme);
        applyStyleSheet();
        ThemeManager::applyTheme();
    }

    cfg().setMinIconSize(ui->minIconSizeSpin->value());

    cfg().setLetterJumpEnabled(ui->letterJumpCheck->isChecked());

    cfg().setMouseClickActivateEnabled(ui->mouseClickActivateCheck->isChecked());
    cfg().setClickShowGroupForMultiWindow(ui->clickShowGroupCheck->isChecked());

    // blocked windows
    QList<BlockedWindowEntry> blocked;
    for (int i = 0; i < ui->blockedTable->rowCount(); ++i) {
        BlockedWindowEntry entry;
        entry.title = ui->blockedTable->item(i, 0)->text();
        entry.className = ui->blockedTable->item(i, 1)->text();
        blocked.append(entry);
    }
    cfg().setBlockedWindows(blocked);
}

void SettingsDialog::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QDialog::changeEvent(event);
}
