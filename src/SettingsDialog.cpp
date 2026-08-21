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
#include <QBitArray>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QGroupBox>
#include <QHeaderView>
#include <QListWidget>
#include <QMessageBox>
#include <QUrl>
#include <QVBoxLayout>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QRegularExpression>
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
    ui->blockedTable->setSelectionMode(QAbstractItemView::ExtendedSelection);

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

    ui->desktopScopeCombo->addItem(tr("Current Desktop Only"),
        static_cast<int>(VirtualDesktopScope::CurrentDesktop));
    ui->desktopScopeCombo->addItem(tr("All Desktops"),
        static_cast<int>(VirtualDesktopScope::AllDesktops));

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
        refreshCacheSize();
    });

    connect(ui->btnCleanLogs, &QPushButton::clicked, this, &SettingsDialog::cleanLogFiles);

    connect(ui->btnConfigBrowse, &QPushButton::clicked, this, [this] {
        QString dir = QFileDialog::getExistingDirectory(this, tr("Select Config Directory"),
            ui->configCustomPathEdit->text());
        if (!dir.isEmpty())
            ui->configCustomPathEdit->setText(dir);
    });

    connect(ui->configLocationCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        bool isCustom = ui->configLocationCombo->itemData(index).toString() == "custom";
        ui->configCustomLabel->setVisible(isCustom);
        ui->configCustomPathEdit->setVisible(isCustom);
        ui->btnConfigBrowse->setVisible(isCustom);
    });

    SettingsStyleHelper::applyTheme(this, ui);

    m_searchResultsList = new QListWidget(this);
    m_searchResultsList->setObjectName("searchResultsList");
    m_searchResultsList->setFrameShape(QFrame::NoFrame);
    m_searchResultsList->setUniformItemSizes(true);
    m_searchResultsList->setTextElideMode(Qt::ElideRight);
    m_searchResultsList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_searchResultsList->hide();
    applySearchResultsTheme();
    m_searchResultsList->installEventFilter(this);
    ui->searchEdit->installEventFilter(this);
    qApp->installEventFilter(this);

    connect(ui->searchEdit, &QLineEdit::textChanged, this, &SettingsDialog::filterPages);
    connect(ui->searchEdit, &QLineEdit::returnPressed, this, [this] {
        if (!m_searchResultsList->isVisible())
            return;
        if (auto* cur = m_searchResultsList->currentItem();
            cur && (cur->flags() & Qt::ItemIsEnabled)) {
            activateSearchResult(cur);
            return;
        }
        for (int i = 0; i < m_searchResultsList->count(); ++i) {
            if (m_searchResultsList->item(i)->flags() & Qt::ItemIsEnabled) {
                activateSearchResult(m_searchResultsList->item(i));
                break;
            }
        }
    });
    connect(m_searchResultsList, &QListWidget::itemClicked, this,
            [this](QListWidgetItem* item) { activateSearchResult(item); });
    connect(ui->navList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0 && row < ui->stackedWidget->count())
            ui->stackedWidget->setCurrentIndex(row);
        if (row == 3)
            refreshLogSize();
        else if (row == 4)
            refreshCacheSize();
        if (row != 2)
            m_hotkeyMgr->cancelAllRecordings();
        if (m_searchResultsList)
            m_searchResultsList->hide();
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
    rebuildSearchIndex();
    ui->navList->setCurrentRow(0);
    qInfo() << "SettingsDialog initialized in" << t.elapsed() << "ms";
}

SettingsDialog::~SettingsDialog() {
    qApp->removeEventFilter(this);
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

    idx = ui->desktopScopeCombo->findData(
        static_cast<int>(m_config->getVirtualDesktopScope()));
    if (idx >= 0) ui->desktopScopeCombo->setCurrentIndex(idx);

    ui->minIconSizeSpin->setValue(m_config->getMinIconSize());
    ui->transparencyCheck->setChecked(m_config->getTransparencyEnabled());
    ui->letterJumpCheck->setChecked(m_config->getLetterJumpEnabled());
    bool mouseClickEnabled = m_config->getMouseClickActivateEnabled();
    ui->mouseClickActivateCheck->setChecked(mouseClickEnabled);
    ui->clickShowGroupCheck->setChecked(mouseClickEnabled && m_config->getClickShowGroupForMultiWindow());
    ui->clickShowGroupCheck->setEnabled(mouseClickEnabled);
    ui->taskbarWheelCheck->setChecked(m_config->getTaskbarWheelEnabled());
    ui->hideUtilityWindowsCheck->setChecked(m_config->getHideUtilityWindows());

    {
        auto flags = m_config->getLogFlags();
        ui->chkLogTrace->setChecked(flags & Util::LogTrace);
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

    {
        ui->configLocationCombo->clear();
        ui->configLocationCombo->addItem(tr("Program Directory"), "program");
        ui->configLocationCombo->addItem(tr("User Config Directory"), "appdata");
        ui->configLocationCombo->addItem(tr("Custom..."), "custom");

        auto info = m_config->configLocationInfo();
        auto location = info.location;
        for (int i = 0; i < ui->configLocationCombo->count(); ++i) {
            if (ui->configLocationCombo->itemData(i).toString() ==
                (location == ConfigLocation::ProgramDir ? "program" :
                 location == ConfigLocation::Custom ? "custom" : "appdata")) {
                ui->configLocationCombo->setCurrentIndex(i);
                break;
            }
        }

        ui->configCustomPathEdit->setText(info.customPath);
        bool isCustom = (location == ConfigLocation::Custom);
        ui->configCustomLabel->setVisible(isCustom);
        ui->configCustomPathEdit->setVisible(isCustom);
        ui->btnConfigBrowse->setVisible(isCustom);
    }

    m_blockedMgr->loadFromConfig();

    refreshCacheSize();
    refreshLogSize();

    m_loadingSettings = false;
    retranslateUi();
}

void SettingsDialog::applySettings() {
    {
        auto typeStr = ui->configLocationCombo->currentData().toString();
        ConfigLocation loc = ConfigLocation::AppData;
        if (typeStr == "program") loc = ConfigLocation::ProgramDir;
        else if (typeStr == "custom") loc = ConfigLocation::Custom;

        if (!m_config->setConfigLocation(loc, ui->configCustomPathEdit->text())) {
            QMessageBox::warning(this, tr("Config Location"),
                                 tr("Failed to change the config location.\n"
                                    "The target directory is invalid or not writable."));
            loadSettings();
            return;
        }
    }

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
        SettingsStyleHelper::applyTheme(this, ui);
        applySearchResultsTheme();
        ThemeManager::applyTheme();
    }

    auto desktopScope = static_cast<VirtualDesktopScope>(
        ui->desktopScopeCombo->currentData().toInt());
    m_config->setVirtualDesktopScope(desktopScope);

    m_config->setTransparencyEnabled(ui->transparencyCheck->isChecked());
    m_config->setMinIconSize(ui->minIconSizeSpin->value());
    m_config->setLetterJumpEnabled(ui->letterJumpCheck->isChecked());
    m_config->setMouseClickActivateEnabled(ui->mouseClickActivateCheck->isChecked());
    m_config->setClickShowGroupForMultiWindow(ui->clickShowGroupCheck->isChecked());
    m_config->setTaskbarWheelEnabled(ui->taskbarWheelCheck->isChecked());
    m_config->setHideUtilityWindows(ui->hideUtilityWindowsCheck->isChecked());

    {
        Util::LogFlags flags = 0;
        if (ui->chkLogTrace->isChecked()) flags |= Util::LogTrace;
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

    ui->generalGroup->setTitle(tr("General"));
    ui->langLabel->setText(tr("Language:"));
    ui->configLocationLabel->setText(tr("Config Location:"));
    ui->configCustomLabel->setText(tr("Custom Path:"));

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
    ui->chkLogTrace->setText(tr("Trace"));
    ui->chkLogTrace->setToolTip(tr("High-frequency verbose logging, typically not needed in daily use"));
    ui->chkLogDebug->setText(tr("Debug"));
    ui->chkLogDebug->setToolTip(tr("General debugging information"));
    ui->chkLogInfo->setText(tr("Info"));
    ui->chkLogInfo->setToolTip(tr("Informational messages about normal operation"));
    ui->chkLogWarn->setText(tr("Warning"));
    ui->chkLogWarn->setToolTip(tr("Non-critical issues that should be reviewed"));
    ui->chkLogError->setText(tr("Error"));
    ui->chkLogError->setToolTip(tr("Errors that may affect functionality"));
    ui->chkLogFatal->setText(tr("Fatal"));
    ui->chkLogFatal->setToolTip(tr("Critical errors that may cause application crashes"));
    ui->logDirLabel->setText(tr("Log Directory:"));
    ui->btnBrowseLogDir->setText(tr("Browse..."));

    ui->cacheGroup->setTitle(tr("Icon Cache"));
    ui->iconCacheCheck->setText(tr("Enable icon cache"));
    ui->cacheDirLabel->setText(tr("Cache Directory:"));
    ui->btnBrowseCacheDir->setText(tr("Browse..."));
    ui->btnClearCache->setText(tr("Clear Cache"));
    ui->btnCleanLogs->setText(tr("Clean Logs"));

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

    ui->letterJumpCheck->setText(tr("Enable letter jump (A-Z)"));
    ui->mouseClickActivateCheck->setText(tr("Activate window on mouse click"));
    ui->clickShowGroupCheck->setText(tr("Show window list for multi-window apps"));
    ui->taskbarWheelCheck->setText(tr("Enable taskbar wheel switching"));
    ui->hideUtilityWindowsCheck->setText(tr("Hide utility windows (Chrome/Electron popups)"));

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

    rebuildSearchIndex();
}

void SettingsDialog::filterPages(const QString& text) {
    clearHighlights();

    if (text.isEmpty()) {
        for (int i = 0; i < ui->navList->count(); ++i)
            ui->navList->item(i)->setHidden(false);
        m_searchResultsList->hide();
        return;
    }

    const int pageCount = qMin(ui->stackedWidget->count(), ui->navList->count());
    m_searchResultsList->clear();

    for (const auto& entry : m_searchEntries) {
        if (!entry.text.contains(text, Qt::CaseInsensitive))
            continue;
        if (!entry.widget || entry.widget->isHidden() || entry.pageIndex >= pageCount)
            continue;
        auto* item = new QListWidgetItem(
            QStringLiteral("%1  —  %2")
                .arg(entry.text, ui->navList->item(entry.pageIndex)->text()),
            m_searchResultsList);
        item->setData(Qt::UserRole,
                      QVariant::fromValue<qintptr>(reinterpret_cast<qintptr>(entry.widget)));
        item->setData(Qt::UserRole + 1, entry.pageIndex);
    }

    if (m_searchResultsList->count() == 0) {
        auto* noMatch = new QListWidgetItem(tr("No matching settings"), m_searchResultsList);
        noMatch->setFlags(Qt::NoItemFlags);
    }

    positionSearchResults();
    m_searchResultsList->show();
    m_searchResultsList->raise();
}

void SettingsDialog::rebuildSearchIndex() {
    m_searchEntries.clear();
    for (int i = 0; i < ui->stackedWidget->count(); ++i)
        collectSearchTexts(ui->stackedWidget->widget(i), i);
}

void SettingsDialog::collectSearchTexts(QWidget* page, int pageIndex) {
    if (!page)
        return;
    const auto children = page->findChildren<QWidget*>();
    for (auto* w : children) {
        QString text;
        if (auto* gb = qobject_cast<QGroupBox*>(w))
            text = gb->title();
        else if (auto* lbl = qobject_cast<QLabel*>(w)) {
            text = lbl->text();
            if (lbl->textFormat() != Qt::PlainText) {
                static const QRegularExpression tagRe(QStringLiteral("<[^>]+>"));
                text = text.replace(tagRe, QStringLiteral(" ")).simplified();
            }
        } else if (auto* cb = qobject_cast<QCheckBox*>(w))
            text = cb->text();
        else if (auto* rb = qobject_cast<QRadioButton*>(w))
            text = rb->text();
        else if (auto* btn = qobject_cast<QPushButton*>(w)) {
            text = btn->text();
            if (text.startsWith(QLatin1Char('+')))
                continue;
        }
        text.remove(QLatin1Char('&'));
        if (text.trimmed().isEmpty())
            continue;
        m_searchEntries.append({w, pageIndex, text});
    }
}

void SettingsDialog::positionSearchResults() {
    const QPoint origin =
        ui->searchEdit->mapTo(this, QPoint(0, ui->searchEdit->height() + 2));
    const int width = ui->searchEdit->width();
    int rowHeight = 28;
    if (m_searchResultsList->count() > 0)
        rowHeight = qMax(rowHeight, m_searchResultsList->sizeHintForRow(0));
    const int height = qMin(240, m_searchResultsList->count() * rowHeight + 10);
    m_searchResultsList->setGeometry(origin.x(), origin.y(), width, height);
}

void SettingsDialog::moveSearchSelection(bool down) {
    const int count = m_searchResultsList->count();
    if (count == 0)
        return;
    const int step = down ? 1 : -1;
    for (int i = m_searchResultsList->currentRow() + step; i >= 0 && i < count; i += step) {
        if (m_searchResultsList->item(i)->flags() & Qt::ItemIsEnabled) {
            m_searchResultsList->setCurrentRow(i);
            return;
        }
    }
}

bool SettingsDialog::eventFilter(QObject* watched, QEvent* event) {
    if (watched == ui->searchEdit && event->type() == QEvent::KeyPress
        && m_searchResultsList->isVisible()) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        switch (keyEvent->key()) {
        case Qt::Key_Escape:
            m_searchResultsList->hide();
            return true;
        case Qt::Key_Up:
            moveSearchSelection(false);
            return true;
        case Qt::Key_Down:
            moveSearchSelection(true);
            return true;
        default:
            break;
        }
    } else if (event->type() == QEvent::MouseButtonPress && m_searchResultsList->isVisible()
               && watched != m_searchResultsList && watched != ui->searchEdit) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        const QPoint globalPos = mouseEvent->globalPosition().toPoint();
        const QRect popupRect(m_searchResultsList->mapToGlobal(QPoint(0, 0)),
                              m_searchResultsList->size());
        if (!popupRect.contains(globalPos))
            m_searchResultsList->hide();
    }
    return QDialog::eventFilter(watched, event);
}

void SettingsDialog::activateSearchResult(QListWidgetItem* item) {
    if (!item || !(item->flags() & Qt::ItemIsEnabled))
        return;
    auto* widget = reinterpret_cast<QWidget*>(item->data(Qt::UserRole).value<qintptr>());
    const int page = item->data(Qt::UserRole + 1).toInt();
    if (!widget || page < 0 || page >= ui->stackedWidget->count())
        return;

    m_searchResultsList->hide();
    ui->navList->setCurrentRow(page);
    scrollToWidget(widget);
    setWidgetHighlighted(widget, true);
}

void SettingsDialog::scrollToWidget(QWidget* widget) {
    for (auto* p = widget->parentWidget(); p; p = p->parentWidget()) {
        if (auto* area = qobject_cast<QScrollArea*>(p)) {
            area->ensureWidgetVisible(widget, 12, 12);
            return;
        }
    }
}

void SettingsDialog::setWidgetHighlighted(QWidget* widget, bool highlighted) {
    if (!widget)
        return;
    if (highlighted) {
        if (!m_originalStyles.contains(widget))
            m_originalStyles.insert(widget, widget->styleSheet());
        if (qobject_cast<QGroupBox*>(widget)) {
            widget->setStyleSheet(
                QStringLiteral("QGroupBox{border: 2px solid #E6B800; border-radius: 6px;}"));
        } else {
            widget->setStyleSheet(QStringLiteral(
                "background-color: #FFF3B0; color: #1A1A1A; border-radius: 4px;"));
        }
    } else {
        widget->setStyleSheet(m_originalStyles.take(widget));
    }
}

void SettingsDialog::clearHighlights() {
    for (auto it = m_originalStyles.constBegin(); it != m_originalStyles.constEnd(); ++it)
        it.key()->setStyleSheet(it.value());
    m_originalStyles.clear();
}

void SettingsDialog::applySearchResultsTheme() {
    const auto& tc = ThemeManager::current();
    m_searchResultsList->setStyleSheet(QStringLiteral(
        "QListWidget#searchResultsList {"
        "  background: %1; color: %2;"
        "  border: 1px solid %3; border-radius: 6px; padding: 2px;"
        "}"
        "QListWidget#searchResultsList::item { padding: 4px 8px; border-radius: 4px; }"
        "QListWidget#searchResultsList::item:selected { background: %4; color: %1; }")
        .arg(tc.bgColor.name(), tc.textColor.name(),
             tc.borderColor.name(), tc.accentColor.name()));
}

namespace {
qint64 calculateDirectorySize(const QString& dirPath) {
    QDir dir(dirPath);
    if (!dir.exists()) return 0;
    qint64 total = 0;
    const auto files = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const auto& fi : files)
        total += fi.size();
    const auto subdirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const auto& sd : subdirs)
        total += calculateDirectorySize(sd.absoluteFilePath());
    return total;
}

QString formatSize(qint64 bytes) {
    if (bytes < 1024)
        return QString::number(bytes) + " B";
    else if (bytes < 1024 * 1024)
        return QString::number(bytes / 1024.0, 'f', 1) + " KB";
    else
        return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + " MB";
}
} // anonymous namespace

void SettingsDialog::refreshCacheSize() {
    qint64 size = calculateDirectorySize(m_config->getIconCacheDirectory());
    ui->lblCacheSize->setText(tr("Cache Size: %1").arg(formatSize(size)));
}

void SettingsDialog::refreshLogSize() {
    QString logDir = PathUtils::resolveAppRelativePath(
        ui->logDirEdit->text().isEmpty() ? m_config->getLogDirectory() : ui->logDirEdit->text(),
        "log");
    qint64 size = calculateDirectorySize(logDir);
    ui->lblLogSize->setText(tr("Log Size: %1").arg(formatSize(size)));
}

void SettingsDialog::cleanLogFiles() {
    auto reply = QMessageBox::question(this, tr("Clean Log Files"),
        tr("Are you sure you want to delete all log files?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    QString logDir = PathUtils::resolveAppRelativePath(
        ui->logDirEdit->text().isEmpty() ? m_config->getLogDirectory() : ui->logDirEdit->text(),
        "log");
    QDir dir(logDir);
    if (!dir.exists()) {
        QMessageBox::information(this, tr("Logs Cleaned"), tr("No log files found."));
        return;
    }

    Util::Logger::closeLog();

    int count = 0;
    const auto files = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const auto& fi : files) {
        if (QFile::remove(fi.absoluteFilePath()))
            ++count;
    }

    Util::Logger::reopenLog();

    QMessageBox::information(this, tr("Logs Cleaned"),
        tr("Deleted %1 log file(s).").arg(count));
    refreshLogSize();
}

void SettingsDialog::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
        QMetaObject::invokeMethod(this, [this] {
            m_hotkeyMgr->buildHotkeyPage(ui->stackedWidget, ui->hotkeyPlaceholder);
            m_hotkeyMgr->loadBindings();
            rebuildSearchIndex();
        }, Qt::QueuedConnection);
    } else if (event->type() == QEvent::WindowDeactivate) {
        m_hotkeyMgr->cancelAllRecordings();
    }
    QDialog::changeEvent(event);
}

void SettingsDialog::resizeEvent(QResizeEvent* event) {
    QDialog::resizeEvent(event);
    if (m_searchResultsList && m_searchResultsList->isVisible())
        positionSearchResults();
}
