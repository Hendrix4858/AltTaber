#include "SettingsDialog.h"
#include "ui_SettingsDialog.h"
#include "utils/ConfigManager.h"
#include "utils/LanguageManager.h"
#include "utils/Startup.h"

#include <QApplication>
#include <QFont>

static const QColor BgColor(35, 35, 35);
static const QColor PanelColor(45, 45, 45);
static const QColor TextColor(220, 220, 220);
static const QColor HighlightColor(60, 60, 60);
static const QColor BorderColor(25, 25, 25);
static const QColor InputBg(50, 50, 50);

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
    ui->navList->addItem(tr("About"));

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

    loadSettings();
    ui->navList->setCurrentRow(0);
}

SettingsDialog::~SettingsDialog() {
    delete ui;
}

void SettingsDialog::applyStyleSheet() {
    setStyleSheet(QString("QDialog { background-color: %1; }").arg(BgColor.name()));

    ui->topBar->setStyleSheet(QString("background-color: %1; border-bottom: 1px solid %2;")
                              .arg(PanelColor.name(), BorderColor.name()));

    ui->titleLabel->setStyleSheet(QString("color: %1; background: transparent; border: none;")
                                  .arg(TextColor.name()));

    ui->searchEdit->setStyleSheet(QString(
        "QLineEdit {"
        "  padding: 6px 10px; border: 1px solid %1; border-radius: 4px;"
        "  background-color: %2; color: %3; font-size: 13px;"
        "}"
        "QLineEdit:focus { border-color: #0078D4; }"
    ).arg(BorderColor.name(), InputBg.name(), TextColor.name()));

    ui->navList->setStyleSheet(QString(
        "QListWidget {"
        "  background-color: %1; color: %2; border-right: 1px solid %3;"
        "  border-top: none; border-left: none; border-bottom: none; outline: none;"
        "  font-size: 13px;"
        "}"
        "QListWidget::item { padding: 10px 16px; border: none; }"
        "QListWidget::item:selected { background-color: %4; color: white; }"
        "QListWidget::item:hover:!selected { background-color: %5; }"
    ).arg(PanelColor.name(), TextColor.name(), BorderColor.name(),
          HighlightColor.name(), QColor(55, 55, 55).name()));

    ui->stackedWidget->setStyleSheet(QString("background-color: %1;").arg(BgColor.name()));

    const QString groupBoxStyle = QString(
        "QGroupBox { color: %1; font-size: 13px; font-weight: bold;"
        "  border: 1px solid %2; border-radius: 6px; margin-top: 12px; padding-top: 16px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; }"
    ).arg(TextColor.name(), BorderColor.name());
    ui->langGroup->setStyleSheet(groupBoxStyle);
    ui->displayGroup->setStyleSheet(groupBoxStyle);

    const QString comboStyle = QString(
        "QComboBox { padding: 6px 10px; border: 1px solid %1; border-radius: 4px;"
        "  background-color: %2; color: %3; font-size: 13px; }"
        "QComboBox:focus { border-color: #0078D4; }"
        "QComboBox::drop-down { border: none; width: 24px; }"
        "QComboBox::down-arrow { width: 0; }"
        "QComboBox QAbstractItemView { background-color: %2; color: %3;"
        "  border: 1px solid %1; selection-background-color: %4; }"
    ).arg(BorderColor.name(), InputBg.name(), TextColor.name(), HighlightColor.name());
    ui->langCombo->setStyleSheet(comboStyle);
    ui->monitorCombo->setStyleSheet(comboStyle);

    ui->langLabel->setStyleSheet(QString("color: %1; font-weight: normal;").arg(TextColor.name()));
    ui->monitorLabel->setStyleSheet(QString("color: %1; font-weight: normal;").arg(TextColor.name()));

    const QString checkStyle = QString(
        "QCheckBox { color: %1; font-size: 13px; spacing: 8px; margin: 4px 0; }"
        "QCheckBox::indicator { width: 16px; height: 16px; }"
    ).arg(TextColor.name());
    ui->startupCheck->setStyleSheet(checkStyle);
    ui->adminCheck->setStyleSheet(checkStyle);

    ui->hotkeyPlaceholder->setStyleSheet(QString("color: #888; font-size: 15px;"));

    ui->aboutDesc->setStyleSheet(QString("color: %1; font-size: 14px;").arg(TextColor.name()));

    ui->bottomBar->setStyleSheet(QString("background-color: %1; border-top: 1px solid %2;")
                                 .arg(PanelColor.name(), BorderColor.name()));

    const QString btnStyle = QString(
        "QPushButton {"
        "  padding: 6px 20px; border: 1px solid %1; border-radius: 4px;"
        "  background-color: %2; color: %3; font-size: 13px; min-width: 70px;"
        "}"
        "QPushButton:hover { background-color: %4; }"
        "QPushButton:pressed { background-color: %5; }"
        "QPushButton:disabled { color: #555; }"
    ).arg(BorderColor.name(), InputBg.name(), TextColor.name(),
          HighlightColor.name(), QColor(70, 70, 70).name());
    ui->btnOk->setStyleSheet(btnStyle);
    ui->btnCancel->setStyleSheet(btnStyle);
    ui->btnApply->setStyleSheet(btnStyle);
}

void SettingsDialog::retranslateUi() {
    setWindowTitle(tr("Settings"));
    ui->titleLabel->setText(tr("Settings"));
    ui->searchEdit->setPlaceholderText(tr("Search..."));

    if (ui->navList->count() >= 4) {
        ui->navList->item(0)->setText(tr("General"));
        ui->navList->item(1)->setText(tr("Display"));
        ui->navList->item(2)->setText(tr("Hotkeys"));
        ui->navList->item(3)->setText(tr("About"));
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
}

void SettingsDialog::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QDialog::changeEvent(event);
}
