#include "SettingsDialog.h"
#include "ui_SettingsDialog.h"
#include "SettingsStyleHelper.h"
#include "HotkeyPageManager.h"
#include "BlockedWindowManager.h"
#include "core/ConfigManager.h"
#include "core/LanguageManager.h"
#include "lifecycle/Startup.h"
#include "core/ThemeManager.h"
#include "hook/KeyboardHooker.h"
#include "utils/PathUtils.h"

#include <QApplication>
#include <QDir>
#include <QFileDialog>
#include <QHeaderView>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QElapsedTimer>
#include <QFileInfo>
#include "lifecycle/Logger.h"
#include "utils/Util.h"

SettingsDialog::SettingsDialog(ConfigManager* config, QWidget* parent)
    : QDialog(parent, Qt::Window | Qt::WindowCloseButtonHint | Qt::WindowMinimizeButtonHint)
    , m_config(config)
    , ui(new Ui::SettingsDialog)
    , m_hotkeyMgr(new HotkeyPageManager(config, this))
    , m_blockedMgr(nullptr) {
    QElapsedTimer t;
    t.start();
    ui->setupUi(this);

    ui->navList->addItem(tr("General"));
    ui->navList->addItem(tr("Display"));
    ui->navList->addItem(tr("Hotkeys"));
    ui->navList->addItem(tr("Logging"));
    ui->navList->addItem(tr("Cache"));
    ui->navList->addItem(tr("Blocked Windows"));
    ui->navList->addItem(tr("About"));

    ui->blockedTable->setColumnCount(6);
    QStringList headers;
    headers << tr("Enabled") << tr("Comment") << tr("Window Title")
            << tr("Class Name") << tr("Process Name") << tr("Process Path");
    ui->blockedTable->setHorizontalHeaderLabels(headers);
    ui->blockedTable->horizontalHeader()->setStretchLastSection(false);
    ui->blockedTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->blockedTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);

    m_blockedMgr = new BlockedWindowManager(config, ui->blockedTable,
                                            ui->btnAddBlocked, ui->btnRemoveBlocked, this);

    m_btnEditBlocked = m_blockedMgr->createEditButton(this);
    m_btnExportBlocked = m_blockedMgr->createExportButton(this);
    m_btnImportBlocked = m_blockedMgr->createImportButton(this);

    if (auto* blockedLayout = ui->blockedGroup->findChild<QHBoxLayout*>("blockedBtnLayout")) {
        blockedLayout->insertWidget(1, m_btnEditBlocked);
        blockedLayout->addWidget(m_btnExportBlocked);
        blockedLayout->addWidget(m_btnImportBlocked);
    }

    ui->themeCombo->addItem(tr("Dark"), Dark);
    ui->themeCombo->addItem(tr("Light"), Light);
    ui->themeCombo->addItem(tr("Follow System"), System);

    ui->langCombo->addItem(tr("Follow System"), "system");
    ui->langCombo->addItem(QStringLiteral("English"), "en");
    ui->langCombo->addItem(QStringLiteral("中文"), "zh_CN");

    connect(ui->btnBrowseLogDir, &QPushButton::clicked, this, [this] {
        auto text = ui->logDirEdit->text();
        QString startDir = text.isEmpty()
            ? QApplication::applicationDirPath() + "/log"
            : PathUtils::resolveAppRelativePath(text, "log");
        QString dir = QFileDialog::getExistingDirectory(this, tr("Select Log Directory"), startDir);
        if (!dir.isEmpty())
            ui->logDirEdit->setText(dir);
    });

    connect(ui->btnBrowseCacheDir, &QPushButton::clicked, this, [this] {
        auto text = ui->cacheDirEdit->text();
        QString startDir = text.isEmpty()
            ? QApplication::applicationDirPath() + "/icon_cache"
            : PathUtils::resolveAppRelativePath(text, "icon_cache");
        QString dir = QFileDialog::getExistingDirectory(this, tr("Select Cache Directory"), startDir);
        if (!dir.isEmpty())
            ui->cacheDirEdit->setText(dir);
    });

    connect(ui->btnClearCache, &QPushButton::clicked, this, [this] {
        auto reply = QMessageBox::question(this, tr("Clear Cache"),
                                            tr("Are you sure you want to clear the icon cache?\n"
                                               "Icons will be re-extracted on next use."),
                                            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            QDir dir(m_config->getIconCacheDirectory());
            if (dir.exists()) {
                dir.removeRecursively();
                QMessageBox::information(this, tr("Cache Cleared"),
                                          tr("Icon cache has been cleared."));
            }
        }
    });

    SettingsStyleHelper::applyTheme(this, ui,
        {m_btnEditBlocked, m_btnExportBlocked, m_btnImportBlocked});

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

    connect(ui->pwaEnabledCheck, &QCheckBox::toggled, this, [this](bool checked) {
        ui->pwaSeparateRadio->setEnabled(checked);
        ui->pwaTagRadio->setEnabled(checked);
    });

    connect(m_hotkeyMgr, &HotkeyPageManager::bindingsChanged, this, [this](bool hasSingleLetter) {
        if (hasSingleLetter) {
            ui->letterJumpCheck->setChecked(false);
            ui->letterJumpCheck->setEnabled(false);
        } else {
            ui->letterJumpCheck->setEnabled(true);
        }
    });

    loadSettings();
    m_hotkeyMgr->buildHotkeyPage(ui->stackedWidget, ui->hotkeyPlaceholder);
    m_hotkeyMgr->loadBindings();
    ui->navList->setCurrentRow(0);
    qInfo() << "SettingsDialog initialized in" << t.elapsed() << "ms";
}

SettingsDialog::~SettingsDialog() {
    delete ui;
}

void SettingsDialog::reject() {
    m_hotkeyMgr->cancelAllRecordings();
    KeyboardHooker::clearRecordingTarget();
    QDialog::reject();
}

bool SettingsDialog::nativeEvent(const QByteArray&, void* message, qintptr*) {
    auto* msg = static_cast<MSG*>(message);
    if (msg->message != KeyboardHooker::recordingMessageId())
        return false;

    quint32 vk, scan;
    DWORD flags;
    Qt::KeyboardModifiers mods;
    if (!KeyboardHooker::tryTakeRecordedKey(vk, scan, flags, mods))
        return false;

    return m_hotkeyMgr->handleRecordedKey(vk, scan, flags, mods);
}

void SettingsDialog::loadSettings() {
    m_loadingSettings = true;

    QString lang = m_config->getLanguage();
    int idx = ui->langCombo->findData(lang);
    if (idx >= 0) ui->langCombo->setCurrentIndex(idx);

    ui->startupCheck->setChecked(Startup::isOn());
    ui->adminCheck->setChecked(m_config->getAlwaysRunAsAdmin());

    idx = ui->monitorCombo->findData(m_config->getDisplayMonitor());
    if (idx >= 0) ui->monitorCombo->setCurrentIndex(idx);

    idx = ui->themeCombo->findData(m_config->getTheme());
    if (idx >= 0) ui->themeCombo->setCurrentIndex(idx);

    ui->minIconSizeSpin->setValue(m_config->getMinIconSize());
    ui->letterJumpCheck->setChecked(m_config->getLetterJumpEnabled());
    bool mouseClickEnabled = m_config->getMouseClickActivateEnabled();
    ui->mouseClickActivateCheck->setChecked(mouseClickEnabled);
    ui->clickShowGroupCheck->setChecked(mouseClickEnabled && m_config->getClickShowGroupForMultiWindow());
    ui->clickShowGroupCheck->setEnabled(mouseClickEnabled);

    {
        auto flags = m_config->getLogFlags();
        ui->chkLogDebug->setChecked(flags & Util::LogDebug);
        ui->chkLogInfo->setChecked(flags & Util::LogInfo);
        ui->chkLogWarn->setChecked(flags & Util::LogWarn);
        ui->chkLogError->setChecked(flags & Util::LogError);
        ui->chkLogFatal->setChecked(flags & Util::LogFatal);
    }
    ui->logDirEdit->setText(m_config->getLogDirectory());
    ui->iconCacheCheck->setChecked(m_config->getIconCacheEnabled());
    ui->cacheDirEdit->setText(m_config->get("IconCacheDirectory", "").toString());

    {
        bool pwaEnabled = m_config->getPwaEnabled();
        ui->pwaEnabledCheck->setChecked(pwaEnabled);
        auto pwaMode = m_config->getPwaMode();
        ui->pwaSeparateRadio->setChecked(pwaMode == PwaMode::SeparateGroups);
        ui->pwaTagRadio->setChecked(pwaMode == PwaMode::TagWithinGroup);
        ui->pwaSeparateRadio->setEnabled(pwaEnabled);
        ui->pwaTagRadio->setEnabled(pwaEnabled);
    }

    m_blockedMgr->loadFromConfig();

    m_loadingSettings = false;
    retranslateUi();
}

void SettingsDialog::applySettings() {
    QString lang = ui->langCombo->currentData().toString();
    m_config->setLanguage(lang);
    switchLanguage(lang);
    retranslateUi();

    bool startup = ui->startupCheck->isChecked();
    if (startup != Startup::isOn())
        Startup::toggle();

    m_config->setAlwaysRunAsAdmin(ui->adminCheck->isChecked());

    auto monitor = static_cast<DisplayMonitor>(ui->monitorCombo->currentData().toInt());
    m_config->setDisplayMonitor(monitor);

    int theme = ui->themeCombo->currentData().toInt();
    if (theme != m_config->getTheme()) {
        m_config->setTheme(theme);
        SettingsStyleHelper::applyTheme(this, ui,
            {m_btnEditBlocked, m_btnExportBlocked, m_btnImportBlocked});
        ThemeManager::applyTheme();
    }

    m_config->setMinIconSize(ui->minIconSizeSpin->value());
    m_config->setLetterJumpEnabled(ui->letterJumpCheck->isChecked());
    m_config->setMouseClickActivateEnabled(ui->mouseClickActivateCheck->isChecked());
    m_config->setClickShowGroupForMultiWindow(ui->clickShowGroupCheck->isChecked());

    {
        Util::LogFlags flags = 0;
        if (ui->chkLogDebug->isChecked()) flags |= Util::LogDebug;
        if (ui->chkLogInfo->isChecked())  flags |= Util::LogInfo;
        if (ui->chkLogWarn->isChecked())  flags |= Util::LogWarn;
        if (ui->chkLogError->isChecked()) flags |= Util::LogError;
        if (ui->chkLogFatal->isChecked()) flags |= Util::LogFatal;
        m_config->setLogFlags(flags);
    }
    m_config->setLogDirectory(ui->logDirEdit->text());
    Util::Logger::reconfigure();

    m_config->setIconCacheEnabled(ui->iconCacheCheck->isChecked());
    m_config->setIconCacheDirectory(ui->cacheDirEdit->text());

    m_config->setPwaEnabled(ui->pwaEnabledCheck->isChecked());
    m_config->setPwaMode(ui->pwaSeparateRadio->isChecked()
                             ? PwaMode::SeparateGroups
                             : PwaMode::TagWithinGroup);

    m_config->setBlockedWindows(m_blockedMgr->collectEntries());
    m_hotkeyMgr->applyBindings();
    m_config->sync();
}

void SettingsDialog::retranslateUi() {
    ui->retranslateUi(this);
    ui->searchEdit->setPlaceholderText(tr("Search..."));

    if (ui->navList->count() >= 7) {
        ui->navList->item(0)->setText(tr("General"));
        ui->navList->item(1)->setText(tr("Display"));
        ui->navList->item(2)->setText(tr("Hotkeys"));
        ui->navList->item(3)->setText(tr("Logging"));
        ui->navList->item(4)->setText(tr("Cache"));
        ui->navList->item(5)->setText(tr("Blocked Windows"));
        ui->navList->item(6)->setText(tr("About"));
    }

    ui->langGroup->setTitle(tr("Language Settings"));
    ui->langLabel->setText(tr("Language:"));

    QString savedLang = ui->langCombo->currentData().toString();
    ui->langCombo->clear();
    ui->langCombo->addItem(tr("Follow System"), "system");
    ui->langCombo->addItem(QStringLiteral("English"), "en");
    ui->langCombo->addItem(QStringLiteral("中文"), "zh_CN");
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
    ui->themeCombo->addItem(tr("Follow System"), System);
    idx = ui->themeCombo->findData(savedTheme);
    if (idx >= 0) ui->themeCombo->setCurrentIndex(idx);

    ui->themeLabel->setText(tr("Theme:"));
    ui->minIconSizeLabel->setText(tr("Min Icon Size:"));

    ui->logGroup->setTitle(tr("Logging"));
    ui->chkLogDebug->setText(tr("Debug"));
    ui->chkLogInfo->setText(tr("Info"));
    ui->chkLogWarn->setText(tr("Warning"));
    ui->chkLogError->setText(tr("Error"));
    ui->chkLogFatal->setText(tr("Fatal"));
    ui->logDirLabel->setText(tr("Log Directory:"));
    ui->btnBrowseLogDir->setText(tr("Browse..."));

    ui->cacheGroup->setTitle(tr("Icon Cache"));
    ui->iconCacheCheck->setText(tr("Enable icon cache"));
    ui->cacheDirLabel->setText(tr("Cache Directory:"));
    ui->btnBrowseCacheDir->setText(tr("Browse..."));
    ui->btnClearCache->setText(tr("Clear Cache"));

    ui->blockedGroup->setTitle(tr("Blocked Windows"));
    if (ui->blockedTable->horizontalHeaderItem(0))
        ui->blockedTable->horizontalHeaderItem(0)->setText(tr("Enabled"));
    if (ui->blockedTable->horizontalHeaderItem(1))
        ui->blockedTable->horizontalHeaderItem(1)->setText(tr("Comment"));
    if (ui->blockedTable->horizontalHeaderItem(2))
        ui->blockedTable->horizontalHeaderItem(2)->setText(tr("Window Title"));
    if (ui->blockedTable->horizontalHeaderItem(3))
        ui->blockedTable->horizontalHeaderItem(3)->setText(tr("Class Name"));
    if (ui->blockedTable->horizontalHeaderItem(4))
        ui->blockedTable->horizontalHeaderItem(4)->setText(tr("Process Name"));
    if (ui->blockedTable->horizontalHeaderItem(5))
        ui->blockedTable->horizontalHeaderItem(5)->setText(tr("Process Path"));
    ui->btnAddBlocked->setText(tr("Add"));
    ui->btnRemoveBlocked->setText(tr("Remove"));
    if (m_btnEditBlocked) m_btnEditBlocked->setText(tr("Edit"));
    if (m_btnExportBlocked) m_btnExportBlocked->setText(tr("Export"));
    if (m_btnImportBlocked) m_btnImportBlocked->setText(tr("Import"));

    ui->letterJumpGroup->setTitle(tr("Letter Jump"));
    ui->letterJumpCheck->setText(tr("Enable letter jump (A-Z)"));
    ui->mouseClickActivateCheck->setText(tr("Activate window on mouse click"));
    ui->clickShowGroupCheck->setText(tr("Show window list for multi-window apps"));

    QString version = QApplication::applicationVersion();
    ui->aboutDesc->setText(
        tr("AltTaber - Window Switcher<br>"
           "Version: %1<br><br>"
           "A modern Alt+Tab replacement for Windows.<br><br>"
           "GitHub: <a href='https://github.com/Hendrix4858/AltTaber' "
           "style='color: #0078D4;'>Hendrix4858/AltTaber</a>")
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

void SettingsDialog::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
        QMetaObject::invokeMethod(this, [this] {
            m_hotkeyMgr->buildHotkeyPage(ui->stackedWidget, ui->hotkeyPlaceholder);
            m_hotkeyMgr->loadBindings();
        }, Qt::QueuedConnection);
    }
    QDialog::changeEvent(event);
}
