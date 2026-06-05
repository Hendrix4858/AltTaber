#include "SettingsDialog.h"
#include "utils/ConfigManager.h"
#include "utils/LanguageManager.h"
#include "utils/Startup.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QEvent>
#include <QApplication>
#include <QFont>

static const QColor BgColor(35, 35, 35);
static const QColor PanelColor(45, 45, 45);
static const QColor TextColor(220, 220, 220);
static const QColor HighlightColor(60, 60, 60);
static const QColor BorderColor(25, 25, 25);
static const QColor InputBg(50, 50, 50);

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(tr("Settings"));
    resize(780, 520);
    setMinimumSize(600, 400);
    setStyleSheet(QString("QDialog { background-color: %1; }").arg(BgColor.name()));
    setupUi();
    loadSettings();
    m_navList->setCurrentRow(0);
}

SettingsDialog::~SettingsDialog() = default;

void SettingsDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Top bar
    auto* topBar = new QWidget(this);
    topBar->setStyleSheet(QString("background-color: %1; border-bottom: 1px solid %2;")
                          .arg(PanelColor.name(), BorderColor.name()));
    auto* topLayout = new QVBoxLayout(topBar);
    topLayout->setContentsMargins(12, 8, 12, 8);

    m_titleLabel = new QLabel(topBar);
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(titleFont.pointSize() + 4);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    m_titleLabel->setStyleSheet(QString("color: %1; background: transparent; border: none;").arg(TextColor.name()));
    topLayout->addWidget(m_titleLabel);

    m_searchEdit = new QLineEdit(topBar);
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setStyleSheet(QString(
        "QLineEdit {"
        "  padding: 6px 10px; border: 1px solid %1; border-radius: 4px;"
        "  background-color: %2; color: %3; font-size: 13px;"
        "}"
        "QLineEdit:focus { border-color: #0078D4; }"
    ).arg(BorderColor.name(), InputBg.name(), TextColor.name()));
    topLayout->addWidget(m_searchEdit);

    mainLayout->addWidget(topBar);

    // Body
    auto* bodyLayout = new QHBoxLayout();
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);

    m_navList = new QListWidget(this);
    m_navList->setFixedWidth(180);
    m_navList->setStyleSheet(QString(
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

    m_stackedWidget = new QStackedWidget(this);
    m_stackedWidget->setStyleSheet(QString("background-color: %1;").arg(BgColor.name()));

    setupGeneralPage();
    setupDisplayPage();
    setupHotkeyPage();
    setupAboutPage();

    m_navList->addItem(tr("General"));
    m_navList->addItem(tr("Display"));
    m_navList->addItem(tr("Hotkeys"));
    m_navList->addItem(tr("About"));

    m_navItems = {{0}, {1}, {2}, {3}};

    bodyLayout->addWidget(m_navList);
    bodyLayout->addWidget(m_stackedWidget, 1);
    mainLayout->addLayout(bodyLayout, 1);

    // Bottom bar
    auto* bottomBar = new QWidget(this);
    bottomBar->setStyleSheet(QString("background-color: %1; border-top: 1px solid %2;")
                             .arg(PanelColor.name(), BorderColor.name()));
    auto* bottomLayout = new QHBoxLayout(bottomBar);
    bottomLayout->setContentsMargins(12, 8, 12, 8);
    bottomLayout->addStretch();

    m_btnOk = new QPushButton(bottomBar);
    m_btnCancel = new QPushButton(bottomBar);
    m_btnApply = new QPushButton(bottomBar);

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
    m_btnOk->setStyleSheet(btnStyle);
    m_btnCancel->setStyleSheet(btnStyle);
    m_btnApply->setStyleSheet(btnStyle);

    bottomLayout->addWidget(m_btnOk);
    bottomLayout->addWidget(m_btnCancel);
    bottomLayout->addWidget(m_btnApply);
    mainLayout->addWidget(bottomBar);

    connect(m_searchEdit, &QLineEdit::textChanged, this, &SettingsDialog::filterPages);
    connect(m_navList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0 && row < m_stackedWidget->count())
            m_stackedWidget->setCurrentIndex(row);
    });
    connect(m_btnOk, &QPushButton::clicked, this, [this] {
        applySettings();
        accept();
    });
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_btnApply, &QPushButton::clicked, this, &SettingsDialog::applySettings);

    retranslateUi();
}

void SettingsDialog::setupGeneralPage() {
    m_generalPage = new QWidget(this);
    auto* layout = new QVBoxLayout(m_generalPage);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);

    auto* langGroup = new QGroupBox(m_generalPage);
    langGroup->setObjectName("langGroup");
    langGroup->setStyleSheet(QString(
        "QGroupBox { color: %1; font-size: 13px; font-weight: bold;"
        "  border: 1px solid %2; border-radius: 6px; margin-top: 12px; padding-top: 16px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; }"
    ).arg(TextColor.name(), BorderColor.name()));
    auto* langLayout = new QFormLayout(langGroup);

    m_langCombo = new QComboBox(langGroup);
    m_langCombo->addItem("English", "en");
    m_langCombo->addItem("中文", "zh_CN");
    const QString comboStyle = QString(
        "QComboBox { padding: 6px 10px; border: 1px solid %1; border-radius: 4px;"
        "  background-color: %2; color: %3; font-size: 13px; }"
        "QComboBox:focus { border-color: #0078D4; }"
        "QComboBox::drop-down { border: none; width: 24px; }"
        "QComboBox::down-arrow { width: 0; }"
        "QComboBox QAbstractItemView { background-color: %2; color: %3;"
        "  border: 1px solid %1; selection-background-color: %4; }"
    ).arg(BorderColor.name(), InputBg.name(), TextColor.name(), HighlightColor.name());
    m_langCombo->setStyleSheet(comboStyle);

    auto* langLabel = new QLabel(tr("Language:"), langGroup);
    langLabel->setStyleSheet(QString("color: %1; font-weight: normal;").arg(TextColor.name()));
    langLayout->addRow(langLabel, m_langCombo);
    layout->addWidget(langGroup);

    m_startupCheck = new QCheckBox(m_generalPage);
    m_startupCheck->setStyleSheet(QString(
        "QCheckBox { color: %1; font-size: 13px; spacing: 8px; }"
        "QCheckBox::indicator { width: 16px; height: 16px; }"
    ).arg(TextColor.name()));
    layout->addWidget(m_startupCheck);
    layout->addStretch();
    m_stackedWidget->addWidget(m_generalPage);
}

void SettingsDialog::setupDisplayPage() {
    m_displayPage = new QWidget(this);
    auto* layout = new QVBoxLayout(m_displayPage);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);

    auto* displayGroup = new QGroupBox(m_displayPage);
    displayGroup->setObjectName("displayGroup");
    displayGroup->setStyleSheet(QString(
        "QGroupBox { color: %1; font-size: 13px; font-weight: bold;"
        "  border: 1px solid %2; border-radius: 6px; margin-top: 12px; padding-top: 16px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; }"
    ).arg(TextColor.name(), BorderColor.name()));
    auto* formLayout = new QFormLayout(displayGroup);

    m_monitorCombo = new QComboBox(displayGroup);
    const QString comboStyle = QString(
        "QComboBox { padding: 6px 10px; border: 1px solid %1; border-radius: 4px;"
        "  background-color: %2; color: %3; font-size: 13px; }"
        "QComboBox:focus { border-color: #0078D4; }"
        "QComboBox::drop-down { border: none; width: 24px; }"
        "QComboBox::down-arrow { width: 0; }"
        "QComboBox QAbstractItemView { background-color: %2; color: %3;"
        "  border: 1px solid %1; selection-background-color: %4; }"
    ).arg(BorderColor.name(), InputBg.name(), TextColor.name(), HighlightColor.name());
    m_monitorCombo->setStyleSheet(comboStyle);

    auto* monitorLabel = new QLabel(tr("Display Monitor:"), displayGroup);
    monitorLabel->setStyleSheet(QString("color: %1; font-weight: normal;").arg(TextColor.name()));
    formLayout->addRow(monitorLabel, m_monitorCombo);
    layout->addWidget(displayGroup);
    layout->addStretch();
    m_stackedWidget->addWidget(m_displayPage);
}

void SettingsDialog::setupHotkeyPage() {
    m_hotkeyPage = new QWidget(this);
    auto* layout = new QVBoxLayout(m_hotkeyPage);
    layout->setContentsMargins(24, 24, 24, 24);

    auto* placeholder = new QLabel(m_hotkeyPage);
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setStyleSheet(QString("color: #888; font-size: 15px;"));
    layout->addWidget(placeholder);
    m_stackedWidget->addWidget(m_hotkeyPage);
}

void SettingsDialog::setupAboutPage() {
    m_aboutPage = new QWidget(this);
    auto* layout = new QVBoxLayout(m_aboutPage);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setAlignment(Qt::AlignCenter);

    m_aboutDesc = new QLabel(m_aboutPage);
    m_aboutDesc->setAlignment(Qt::AlignCenter);
    m_aboutDesc->setStyleSheet(QString("color: %1; font-size: 14px;").arg(TextColor.name()));
    m_aboutDesc->setTextFormat(Qt::RichText);
    m_aboutDesc->setWordWrap(true);
    m_aboutDesc->setOpenExternalLinks(true);
    layout->addWidget(m_aboutDesc);
    m_stackedWidget->addWidget(m_aboutPage);
}

void SettingsDialog::retranslateUi() {
    m_titleLabel->setText(tr("Settings"));
    m_searchEdit->setPlaceholderText(tr("Search..."));

    if (m_navList->count() >= 4) {
        m_navList->item(0)->setText(tr("General"));
        m_navList->item(1)->setText(tr("Display"));
        m_navList->item(2)->setText(tr("Hotkeys"));
        m_navList->item(3)->setText(tr("About"));
    }

    m_btnOk->setText(tr("OK"));
    m_btnCancel->setText(tr("Cancel"));
    m_btnApply->setText(tr("Apply"));

    auto langGroup = m_generalPage->findChild<QGroupBox*>("langGroup");
    if (langGroup) {
        langGroup->setTitle(tr("Language Settings"));
        auto labels = langGroup->findChildren<QLabel*>();
        if (!labels.isEmpty()) labels.first()->setText(tr("Language:"));
    }

    QString savedLang = m_langCombo->currentData().toString();
    m_langCombo->clear();
    m_langCombo->addItem(tr("English"), "en");
    m_langCombo->addItem("中文", "zh_CN");
    int idx = m_langCombo->findData(savedLang);
    if (idx >= 0) m_langCombo->setCurrentIndex(idx);

    m_startupCheck->setText(tr("Start with Windows"));

    auto displayGroup = m_displayPage->findChild<QGroupBox*>("displayGroup");
    if (displayGroup) {
        displayGroup->setTitle(tr("Display Settings"));
        auto labels = displayGroup->findChildren<QLabel*>();
        if (!labels.isEmpty()) labels.first()->setText(tr("Display Monitor:"));
    }

    int savedMonitor = m_monitorCombo->currentData().toInt();
    m_monitorCombo->clear();
    m_monitorCombo->addItem(tr("Primary Monitor"), PrimaryMonitor);
    m_monitorCombo->addItem(tr("Mouse Monitor"), MouseMonitor);
    idx = m_monitorCombo->findData(savedMonitor);
    if (idx >= 0) m_monitorCombo->setCurrentIndex(idx);

    auto hotkeyPlaceholder = m_hotkeyPage->findChildren<QLabel*>();
    if (!hotkeyPlaceholder.isEmpty())
        hotkeyPlaceholder.first()->setText(tr("Hotkey settings coming soon..."));

    QString version = QApplication::applicationVersion();
    m_aboutDesc->setText(tr("AltTaber - Window Switcher<br>"
                            "Version: %1<br><br>"
                            "A modern Alt+Tab replacement for Windows.<br><br>"
                            "GitHub: <a href='https://github.com/MrBeanCpp/AltTaber' "
                            "style='color: #0078D4;'>MrBeanCpp/AltTaber</a>")
                         .arg(version));
}

void SettingsDialog::filterPages(const QString& text) {
    for (int i = 0; i < m_navList->count(); ++i) {
        auto* item = m_navList->item(i);
        item->setHidden(!text.isEmpty() &&
                        !item->text().contains(text, Qt::CaseInsensitive));
    }

    if (m_navList->currentItem() && m_navList->currentItem()->isHidden()) {
        for (int i = 0; i < m_navList->count(); ++i) {
            if (!m_navList->item(i)->isHidden()) {
                m_navList->setCurrentRow(i);
                return;
            }
        }
    }
}

void SettingsDialog::loadSettings() {
    m_loadingSettings = true;

    QString lang = cfg().getLanguage();
    int idx = m_langCombo->findData(lang);
    if (idx >= 0) m_langCombo->setCurrentIndex(idx);

    m_startupCheck->setChecked(Startup::isOn());

    idx = m_monitorCombo->findData(cfg().getDisplayMonitor());
    if (idx >= 0) m_monitorCombo->setCurrentIndex(idx);

    m_loadingSettings = false;

    retranslateUi();
}

void SettingsDialog::applySettings() {
    QString lang = m_langCombo->currentData().toString();
    cfg().setLanguage(lang);
    switchLanguage(lang);
    retranslateUi();

    bool startup = m_startupCheck->isChecked();
    if (startup != Startup::isOn())
        Startup::toggle();

    auto monitor = static_cast<DisplayMonitor>(m_monitorCombo->currentData().toInt());
    cfg().setDisplayMonitor(monitor);

}

void SettingsDialog::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QDialog::changeEvent(event);
}
