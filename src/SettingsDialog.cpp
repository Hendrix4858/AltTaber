#include "SettingsDialog.h"
#include "ui_SettingsDialog.h"
#include "core/ConfigManager.h"
#include "core/LanguageManager.h"
#include "lifecycle/Startup.h"
#include "core/ThemeManager.h"
#include "hook/HotkeyRecorder.h"
#include "hook/KeyboardHooker.h"
#include "AddBlockedDialog.h"

#include <QApplication>
#include <QFileDialog>
#include <QFont>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QMessageBox>
#include <QDir>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QPushButton>

#include "lifecycle/Logger.h"
#include <QElapsedTimer>
#include <QFileInfo>
#include <QFile>
#include <QTableWidgetItem>
#include "utils/Util.h"

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent, Qt::Window | Qt::WindowCloseButtonHint | Qt::WindowMinimizeButtonHint)
    , ui(new Ui::SettingsDialog) {
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

    // Configure blocked table columns
    ui->blockedTable->setColumnCount(6);
    QStringList headers;
    headers << tr("Enabled") << tr("Comment") << tr("Window Title")
            << tr("Class Name") << tr("Process Name") << tr("Process Path");
    ui->blockedTable->setHorizontalHeaderLabels(headers);
    ui->blockedTable->horizontalHeader()->setStretchLastSection(false);
    ui->blockedTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->blockedTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);

    // Create programmatic buttons for blocked window actions
    m_btnEditBlocked = new QPushButton(tr("Edit"), this);
    m_btnExportBlocked = new QPushButton(tr("Export"), this);
    m_btnImportBlocked = new QPushButton(tr("Import"), this);

    // Add to the existing button layout (find blockedBtnLayout)
    if (auto* blockedLayout = ui->blockedGroup->findChild<QHBoxLayout*>("blockedBtnLayout")) {
        blockedLayout->insertWidget(1, m_btnEditBlocked);
        blockedLayout->addWidget(m_btnExportBlocked);
        blockedLayout->addWidget(m_btnImportBlocked);
    }
    // theme combo
    ui->themeCombo->addItem(tr("Dark"), Dark);
    ui->themeCombo->addItem(tr("Light"), Light);
    ui->themeCombo->addItem(tr("Follow System"), System);

    // language combo
    ui->langCombo->addItem(tr("Follow System"), "system");
    ui->langCombo->addItem(QStringLiteral("English"), "en");
    ui->langCombo->addItem(QStringLiteral("中文"), "zh_CN");

    // log level combo
    ui->logLevelCombo->addItem(tr("All"), static_cast<int>(Util::LogLevel::All));
    ui->logLevelCombo->addItem(tr("Debug"), static_cast<int>(Util::LogLevel::Debug));
    ui->logLevelCombo->addItem(tr("Info"), static_cast<int>(Util::LogLevel::Info));
    ui->logLevelCombo->addItem(tr("Warning"), static_cast<int>(Util::LogLevel::Warning));
    ui->logLevelCombo->addItem(tr("Error"), static_cast<int>(Util::LogLevel::Error));
    ui->logLevelCombo->addItem(tr("Fatal"), static_cast<int>(Util::LogLevel::Fatal));
    ui->logLevelCombo->addItem(tr("None"), static_cast<int>(Util::LogLevel::None));

    // browse log directory
    connect(ui->btnBrowseLogDir, &QPushButton::clicked, this, [this] {
        QString dir = QFileDialog::getExistingDirectory(this, tr("Select Log Directory"),
                                                        ui->logDirEdit->text().isEmpty()
                                                            ? QApplication::applicationDirPath() + "/log"
                                                            : ui->logDirEdit->text());
        if (!dir.isEmpty())
            ui->logDirEdit->setText(dir);
    });

    // browse cache directory
    connect(ui->btnBrowseCacheDir, &QPushButton::clicked, this, [this] {
        QString dir = QFileDialog::getExistingDirectory(this, tr("Select Cache Directory"),
                                                        ui->cacheDirEdit->text().isEmpty()
                                                            ? QApplication::applicationDirPath() + "/icon_cache"
                                                            : ui->cacheDirEdit->text());
        if (!dir.isEmpty())
            ui->cacheDirEdit->setText(dir);
    });

    // clear icon cache
    connect(ui->btnClearCache, &QPushButton::clicked, this, [this] {
        auto reply = QMessageBox::question(this, tr("Clear Cache"),
                                            tr("Are you sure you want to clear the icon cache?\n"
                                               "Icons will be re-extracted on next use."),
                                            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            QDir dir(cfg().getIconCacheDirectory());
            if (dir.exists()) {
                dir.removeRecursively();
                QMessageBox::information(this, tr("Cache Cleared"),
                                          tr("Icon cache has been cleared."));
            }
        }
    });

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

    // Add blocked rule via dialog
    connect(ui->btnAddBlocked, &QPushButton::clicked, this, [this] {
        AddBlockedDialog dlg(this);
        connect(&dlg, &AddBlockedDialog::fromCurrentRequested, this, [&dlg]() {
            HWND fore = GetForegroundWindow();
            BlockedWindowEntry entry;
            entry.title = Util::getWindowTitle(fore);
            entry.className = Util::getClassName(fore);
            auto path = Util::getWindowProcessPath(fore);
            entry.processName = QFileInfo(path).fileName();
            entry.processPath = path;
            entry.enabled = true;
            dlg.setFields(entry);
        });
        if (dlg.exec() == QDialog::Accepted) {
            auto entry = dlg.result();
            int row = ui->blockedTable->rowCount();
            ui->blockedTable->insertRow(row);
            auto* enabledItem = new QTableWidgetItem();
            enabledItem->setFlags(enabledItem->flags() | Qt::ItemIsUserCheckable);
            enabledItem->setCheckState(entry.enabled ? Qt::Checked : Qt::Unchecked);
            ui->blockedTable->setItem(row, 0, enabledItem);
            ui->blockedTable->setItem(row, 1, new QTableWidgetItem(entry.comment));
            ui->blockedTable->setItem(row, 2, new QTableWidgetItem(entry.title));
            ui->blockedTable->setItem(row, 3, new QTableWidgetItem(entry.className));
            ui->blockedTable->setItem(row, 4, new QTableWidgetItem(entry.processName));
            ui->blockedTable->setItem(row, 5, new QTableWidgetItem(entry.processPath));
        }
    });

    // Edit blocked rule
    connect(m_btnEditBlocked, &QPushButton::clicked, this, [this] {
        int row = ui->blockedTable->currentRow();
        if (row < 0) return;
        BlockedWindowEntry entry;
        entry.enabled = ui->blockedTable->item(row, 0)->checkState() == Qt::Checked;
        entry.comment = ui->blockedTable->item(row, 1)->text();
        entry.title = ui->blockedTable->item(row, 2)->text();
        entry.className = ui->blockedTable->item(row, 3)->text();
        entry.processName = ui->blockedTable->item(row, 4)->text();
        entry.processPath = ui->blockedTable->item(row, 5)->text();

        AddBlockedDialog dlg(this);
        dlg.setFields(entry);
        connect(&dlg, &AddBlockedDialog::fromCurrentRequested, this, [&dlg]() {
            HWND fore = GetForegroundWindow();
            BlockedWindowEntry entry;
            entry.title = Util::getWindowTitle(fore);
            entry.className = Util::getClassName(fore);
            auto path = Util::getWindowProcessPath(fore);
            entry.processName = QFileInfo(path).fileName();
            entry.processPath = path;
            entry.enabled = true;
            dlg.setFields(entry);
        });
        if (dlg.exec() == QDialog::Accepted) {
            auto result = dlg.result();
            ui->blockedTable->item(row, 0)->setCheckState(result.enabled ? Qt::Checked : Qt::Unchecked);
            ui->blockedTable->item(row, 1)->setText(result.comment);
            ui->blockedTable->item(row, 2)->setText(result.title);
            ui->blockedTable->item(row, 3)->setText(result.className);
            ui->blockedTable->item(row, 4)->setText(result.processName);
            ui->blockedTable->item(row, 5)->setText(result.processPath);
        }
    });

    // Remove blocked rule
    connect(ui->btnRemoveBlocked, &QPushButton::clicked, this, [this] {
        int row = ui->blockedTable->currentRow();
        if (row >= 0)
            ui->blockedTable->removeRow(row);
    });

    // Export blocked rules
    connect(m_btnExportBlocked, &QPushButton::clicked, this, [this] {
        QString filePath = QFileDialog::getSaveFileName(this, tr("Export Blocked Rules"),
                                                         QString(), tr("JSON Files (*.json)"));
        if (filePath.isEmpty()) return;
        QJsonArray arr;
        for (int i = 0; i < ui->blockedTable->rowCount(); ++i) {
            QJsonObject obj;
            obj["enabled"] = ui->blockedTable->item(i, 0)->checkState() == Qt::Checked;
            obj["comment"] = ui->blockedTable->item(i, 1)->text();
            obj["title"] = ui->blockedTable->item(i, 2)->text();
            obj["class"] = ui->blockedTable->item(i, 3)->text();
            obj["processName"] = ui->blockedTable->item(i, 4)->text();
            obj["processPath"] = ui->blockedTable->item(i, 5)->text();
            arr.append(obj);
        }
        QFile file(filePath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
            file.close();
            QMessageBox::information(this, tr("Export"), tr("Exported %1 rules.").arg(arr.size()));
        }
    });

    // Import blocked rules
    connect(m_btnImportBlocked, &QPushButton::clicked, this, [this] {
        QString filePath = QFileDialog::getOpenFileName(this, tr("Import Blocked Rules"),
                                                         QString(), tr("JSON Files (*.json)"));
        if (filePath.isEmpty()) return;
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) return;
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        file.close();
        if (!doc.isArray()) {
            QMessageBox::warning(this, tr("Import"), tr("Invalid format."));
            return;
        }
        QJsonArray arr = doc.array();
        int imported = 0;
        for (const auto& val : arr) {
            QJsonObject obj = val.toObject();
            QString title = obj["title"].toString();
            QString className = obj["class"].toString();
            QString processName = obj["processName"].toString();
            QString processPath = obj["processPath"].toString();
            if (title.isEmpty() && className.isEmpty() && processName.isEmpty() && processPath.isEmpty())
                continue;
            int row = ui->blockedTable->rowCount();
            ui->blockedTable->insertRow(row);
            auto* enabledItem = new QTableWidgetItem();
            enabledItem->setFlags(enabledItem->flags() | Qt::ItemIsUserCheckable);
            enabledItem->setCheckState(obj.value("enabled").toBool(true) ? Qt::Checked : Qt::Unchecked);
            ui->blockedTable->setItem(row, 0, enabledItem);
            ui->blockedTable->setItem(row, 1, new QTableWidgetItem(obj["comment"].toString()));
            ui->blockedTable->setItem(row, 2, new QTableWidgetItem(title));
            ui->blockedTable->setItem(row, 3, new QTableWidgetItem(className));
            ui->blockedTable->setItem(row, 4, new QTableWidgetItem(processName));
            ui->blockedTable->setItem(row, 5, new QTableWidgetItem(processPath));
            imported++;
        }
        QMessageBox::information(this, tr("Import"), tr("Imported %1 rules.").arg(imported));
    });

    loadSettings();
    buildHotkeyPage();
    loadHotkeyBindings();
    ui->navList->setCurrentRow(0);
    qInfo() << "SettingsDialog initialized in" << t.elapsed() << "ms";
}

SettingsDialog::~SettingsDialog() {
    delete ui;
}

void SettingsDialog::reject() {
    for (auto it = m_recorders.begin(); it != m_recorders.end(); ++it)
        it.value()->cancelRecording();
    KeyboardHooker::clearRecordingTarget();
    QDialog::reject();
}

bool SettingsDialog::nativeEvent(const QByteArray&, void* message, qintptr*) {
    auto* msg = static_cast<MSG*>(message);
    if (msg->message != KeyboardHooker::recordingMessageId())
        return false;
    qDebug() << "[RecordRaw] nativeEvent msg=" << msg->message;

    quint32 vk, scan;
    DWORD flags;
    Qt::KeyboardModifiers mods;
    if (!KeyboardHooker::tryTakeRecordedKey(vk, scan, flags, mods))
        return false;

    for (auto it = m_recorders.begin(); it != m_recorders.end(); ++it) {
        if (!it.value()->isRecording()) continue;

        if (vk == VK_CONTROL || vk == VK_SHIFT || vk == VK_MENU ||
            vk == VK_LCONTROL || vk == VK_RCONTROL ||
            vk == VK_LSHIFT || vk == VK_RSHIFT ||
            vk == VK_LMENU || vk == VK_RMENU ||
            vk == VK_LWIN || vk == VK_RWIN) {
            return true;
        }
        if (vk == VK_ESCAPE) {
            it.value()->cancelRecording();
            return true;
        }

        HotkeyBinding b;
        b.modifiers = mods;
        b.vkCode = vk;
        b.scanCode = scan;
        b.extended = (flags & LLKHF_EXTENDED) != 0;
        b.mode = HotkeyBinding::KeyMode::Physical;

        qDebug() << "[RecordRaw] finishRecording vk=" << vk << "sc=" << scan
                 << "ext=" << b.extended << b.toString();
        it.value()->finishRecording(b);
        return true;
    }
    return false;
}

void SettingsDialog::buildHotkeyPage() {
    // Clear existing content from hotkeyPage
    auto* hotkeyPage = ui->stackedWidget->widget(2);
    auto* oldLayout = hotkeyPage->layout();
    if (oldLayout) {
        QLayoutItem* item;
        while ((item = oldLayout->takeAt(0)) != nullptr) {
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }
        delete oldLayout;
    }

    auto* scrollArea = new QScrollArea(hotkeyPage);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto* scrollContent = new QWidget();
    auto* contentLayout = new QVBoxLayout(scrollContent);
    contentLayout->setSpacing(8);
    contentLayout->setContentsMargins(12, 12, 12, 12);

    const auto& c = ThemeManager::current();
    scrollContent->setStyleSheet(QString("color: %1;").arg(c.textColor.name()));

    // Display order: Global first, then Overlay
    static const HotkeyAction displayOrder[] = {
        HotkeyAction::SwitchToNextWindow,
        HotkeyAction::SwitchToPreviousWindow,
        HotkeyAction::CycleProcessWindows,
        HotkeyAction::SwitchProcessWindow,
        HotkeyAction::TogglePause,
        HotkeyAction::ShowSwitcherStayOpen,
        HotkeyAction::EnterGroupMode,
        HotkeyAction::CycleForward,
        HotkeyAction::CycleBackward,
        HotkeyAction::MoveSelectionUp,
        HotkeyAction::MoveSelectionDown,
        HotkeyAction::ActivateSelected,
        HotkeyAction::DismissSwitcher,
    };
    auto makeScopeHeader = [&](const QString& text) -> QLabel* {
        auto* header = new QLabel(text, scrollContent);
        header->setStyleSheet(QString("color: %1; font-weight: bold; font-size: 13px; padding: 8px 0 2px 0;")
                              .arg(c.accentColor.name()));
        return header;
    };

    // Create a recorder for each action, grouped by scope
    HotkeyScope lastScope = HotkeyScope::Global;
    bool firstGlobal = true;
    for (auto action : displayOrder) {
        auto scope = hotkeyActionScope(action);
        if (scope != lastScope) {
            contentLayout->addWidget(makeScopeHeader(tr("Overlay Hotkeys (only when overlay is visible)")));
            lastScope = scope;
        }
        if (firstGlobal) {
            contentLayout->addWidget(makeScopeHeader(tr("Global Hotkeys (always active)")));
            firstGlobal = false;
        }
        auto* recorder = new HotkeyRecorder(action, scrollContent);
        m_recorders[action] = recorder;
        contentLayout->addWidget(recorder);

        // Connect conflict detection
        connect(recorder, &HotkeyRecorder::bindingsChanged, this, [this, action, recorder](HotkeyAction, const QList<HotkeyBinding>& bindings) {
            if (m_resolvingConflict) {
                checkLetterJumpConflict();
                return;
            }
            m_resolvingConflict = true;

            HotkeyAction conflictAction;
            int conflictIndex;
            for (const auto& b : bindings) {
                if (checkConflict(action, b, conflictAction, conflictIndex)) {
                    auto result = QMessageBox::warning(this, tr("冲突"),
                        tr("快捷键 \"%1\" 已被 \"%2\" 使用。\n是否覆盖？")
                            .arg(b.toString(), hotkeyActionDisplayName(conflictAction)),
                        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
                    if (result == QMessageBox::No) {
                        recorder->rollback();
                        m_resolvingConflict = false;
                        checkLetterJumpConflict();
                        return;
                    }
                    auto* conflictRecorder = m_recorders.value(conflictAction);
                    if (conflictRecorder) {
                        auto conflictBinds = conflictRecorder->bindings();
                        conflictBinds.removeAt(conflictIndex);
                        conflictRecorder->blockSignals(true);
                        conflictRecorder->setBindings(conflictBinds);
                        conflictRecorder->blockSignals(false);
                    }
                }
            }

            m_resolvingConflict = false;
            checkLetterJumpConflict();
        });
    }

    // SwitchToNextWindow empty warning label
    m_showSwitcherWarning = new QLabel(tr("Warning: Switch to Next Window has no hotkey assigned, AltTaber cannot be activated"), scrollContent);
    m_showSwitcherWarning->setStyleSheet("color: red; font-weight: bold; padding: 4px 0;");
    m_showSwitcherWarning->setVisible(false);
    contentLayout->addWidget(m_showSwitcherWarning);

    // Reset to defaults button
    contentLayout->addSpacing(12);
    auto* resetBtn = new QPushButton(tr("Reset to Defaults"), scrollContent);
    connect(resetBtn, &QPushButton::clicked, this, [this]() {
        auto reply = QMessageBox::question(this, tr("Reset Hotkeys"),
                                            tr("Reset all hotkeys to their default values?"),
                                            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            for (auto it = m_recorders.begin(); it != m_recorders.end(); ++it)
                it.value()->setBindings({});
            cfg().resetHotkeys();
            loadHotkeyBindings();
        }
    });
    contentLayout->addWidget(resetBtn);
    contentLayout->addStretch();

    scrollArea->setWidget(scrollContent);

    auto* pageLayout = new QVBoxLayout(hotkeyPage);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->addWidget(scrollArea);
}

void SettingsDialog::loadHotkeyBindings() {
    auto bindings = cfg().getHotkeyBindings();
    for (auto it = m_recorders.begin(); it != m_recorders.end(); ++it) {
        if (bindings.contains(it.key()))
            it.value()->setBindings(bindings[it.key()]);
        else
            it.value()->setBindings({});
    }
    checkLetterJumpConflict();

    if (m_showSwitcherWarning) {
        auto showBinds = bindings.value(HotkeyAction::SwitchToNextWindow);
        m_showSwitcherWarning->setVisible(
            bindings.contains(HotkeyAction::SwitchToNextWindow) && showBinds.isEmpty());
    }
}

void SettingsDialog::applyHotkeyBindings() {
    HotkeyBindings bindings;
    for (auto it = m_recorders.begin(); it != m_recorders.end(); ++it) {
        bindings[it.key()] = it.value()->bindings();
    }
    cfg().setHotkeyBindings(bindings);
}

bool SettingsDialog::checkConflict(HotkeyAction action, const HotkeyBinding& binding,
                                    HotkeyAction& conflictAction, int& conflictIndex) const {
    auto scope = hotkeyActionScope(action);
    for (auto it = m_recorders.begin(); it != m_recorders.end(); ++it) {
        if (it.key() == action) continue;
        if (hotkeyActionScope(it.key()) != scope) continue;
        auto binds = it.value()->bindings();
        for (int i = 0; i < binds.size(); ++i) {
            if (binds[i] == binding) {
                conflictAction = it.key();
                conflictIndex = i;
                return true;
            }
        }
    }
    return false;
}

void SettingsDialog::checkLetterJumpConflict() {
    bool hasLetter = false;
    for (auto it = m_recorders.begin(); it != m_recorders.end(); ++it) {
        for (const auto& b : it.value()->bindings()) {
            if (b.isSingleLetter()) {
                hasLetter = true;
                break;
            }
        }
        if (hasLetter) break;
    }

    if (hasLetter) {
        ui->letterJumpCheck->setChecked(false);
        ui->letterJumpCheck->setEnabled(false);
    } else {
        ui->letterJumpCheck->setEnabled(true);
    }
}

void SettingsDialog::applyStyleSheet() {
    const auto& c = ThemeManager::current();

    setStyleSheet(QString("QDialog { background-color: %1; }").arg(c.bgColor.name()));

    ui->topBar->setStyleSheet(QString("background-color: %1; border-bottom: 1px solid %2;")
                              .arg(c.panelColor.name(), c.borderColor.name()));

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
    ui->logLevelCombo->setStyleSheet(comboStyle);

    ui->langLabel->setStyleSheet(QString("color: %1; font-weight: normal;").arg(c.textColor.name()));
    ui->monitorLabel->setStyleSheet(QString("color: %1; font-weight: normal;").arg(c.textColor.name()));
    ui->themeLabel->setStyleSheet(QString("color: %1; font-weight: normal;").arg(c.textColor.name()));
    ui->minIconSizeLabel->setStyleSheet(QString("color: %1; font-weight: normal;").arg(c.textColor.name()));
    ui->logLevelLabel->setStyleSheet(QString("color: %1; font-weight: normal;").arg(c.textColor.name()));
    ui->logDirLabel->setStyleSheet(QString("color: %1; font-weight: normal;").arg(c.textColor.name()));
    ui->cacheDirLabel->setStyleSheet(QString("color: %1; font-weight: normal;").arg(c.textColor.name()));

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
    if (m_btnEditBlocked) m_btnEditBlocked->setStyleSheet(btnStyle);
    if (m_btnExportBlocked) m_btnExportBlocked->setStyleSheet(btnStyle);
    if (m_btnImportBlocked) m_btnImportBlocked->setStyleSheet(btnStyle);
    ui->btnBrowseLogDir->setStyleSheet(btnStyle);
    ui->btnBrowseCacheDir->setStyleSheet(btnStyle);
    ui->btnClearCache->setStyleSheet(btnStyle);

    // Blocked table style
    ui->blockedTable->setStyleSheet(QString(
        "QTableWidget { background-color: %1; color: %2; border: 1px solid %3;"
        "  gridline-color: %3; font-size: 13px; }"
        "QTableWidget::item:selected { background-color: %4; color: white; }"
        "QHeaderView::section { background-color: %5; color: %2;"
        "  border: 1px solid %3; padding: 4px; font-weight: bold; }"
    ).arg(c.inputBg.name(), c.textColor.name(), c.borderColor.name(),
          c.highlightColor.name(), c.panelColor.name()));

    // log directory line edit
    ui->logDirEdit->setStyleSheet(QString(
        "QLineEdit { padding: 6px 10px; border: 1px solid %1; border-radius: 4px;"
        "  background-color: %2; color: %3; font-size: 13px; }"
        "QLineEdit:focus { border-color: %4; }"
    ).arg(c.borderColor.name(), c.inputBg.name(), c.textColor.name(), c.accentColor.name()));
    ui->cacheDirEdit->setStyleSheet(ui->logDirEdit->styleSheet());

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
    ui->logLevelLabel->setText(tr("Log Level:"));

    int savedLogLevel = ui->logLevelCombo->currentData().toInt();
    ui->logLevelCombo->clear();
    ui->logLevelCombo->addItem(tr("All"), static_cast<int>(Util::LogLevel::All));
    ui->logLevelCombo->addItem(tr("Debug"), static_cast<int>(Util::LogLevel::Debug));
    ui->logLevelCombo->addItem(tr("Info"), static_cast<int>(Util::LogLevel::Info));
    ui->logLevelCombo->addItem(tr("Warning"), static_cast<int>(Util::LogLevel::Warning));
    ui->logLevelCombo->addItem(tr("Error"), static_cast<int>(Util::LogLevel::Error));
    ui->logLevelCombo->addItem(tr("Fatal"), static_cast<int>(Util::LogLevel::Fatal));
    ui->logLevelCombo->addItem(tr("None"), static_cast<int>(Util::LogLevel::None));
    int llIdx = ui->logLevelCombo->findData(savedLogLevel);
    if (llIdx >= 0) ui->logLevelCombo->setCurrentIndex(llIdx);

    ui->logDirLabel->setText(tr("Log Directory:"));
    ui->logDirEdit->setPlaceholderText(tr("Leave empty for default"));
    ui->btnBrowseLogDir->setText(tr("Browse..."));

    ui->cacheGroup->setTitle(tr("Icon Cache"));
    ui->iconCacheCheck->setText(tr("Enable icon cache"));
    ui->cacheDirLabel->setText(tr("Cache Directory:"));
    ui->cacheDirEdit->setPlaceholderText(tr("Leave empty for default"));
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
    ui->aboutDesc->setText(tr("AltTaber - Window Switcher<br>"
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
    int logLevelIdx = ui->logLevelCombo->findData(static_cast<int>(cfg().getLogLevel()));
    if (logLevelIdx >= 0) ui->logLevelCombo->setCurrentIndex(logLevelIdx);
    ui->logDirEdit->setText(cfg().getLogDirectory());

    ui->iconCacheCheck->setChecked(cfg().getIconCacheEnabled());
    ui->cacheDirEdit->setText(cfg().getIconCacheDirectory());

    // blocked windows
    ui->blockedTable->setRowCount(0);
    auto blocked = cfg().getBlockedWindows();
    for (const auto& entry : blocked) {
        int row = ui->blockedTable->rowCount();
        ui->blockedTable->insertRow(row);
        auto* enabledItem = new QTableWidgetItem();
        enabledItem->setFlags(enabledItem->flags() | Qt::ItemIsUserCheckable);
        enabledItem->setCheckState(entry.enabled ? Qt::Checked : Qt::Unchecked);
        ui->blockedTable->setItem(row, 0, enabledItem);
        ui->blockedTable->setItem(row, 1, new QTableWidgetItem(entry.comment));
        ui->blockedTable->setItem(row, 2, new QTableWidgetItem(entry.title));
        ui->blockedTable->setItem(row, 3, new QTableWidgetItem(entry.className));
        ui->blockedTable->setItem(row, 4, new QTableWidgetItem(entry.processName));
        ui->blockedTable->setItem(row, 5, new QTableWidgetItem(entry.processPath));
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
    cfg().setLogLevel(static_cast<Util::LogLevel>(ui->logLevelCombo->currentData().toInt()));
    cfg().setLogDirectory(ui->logDirEdit->text());
    Util::Logger::reconfigure();

    cfg().setIconCacheEnabled(ui->iconCacheCheck->isChecked());
    cfg().setIconCacheDirectory(ui->cacheDirEdit->text());

    // blocked windows
    QList<BlockedWindowEntry> blocked;
    for (int i = 0; i < ui->blockedTable->rowCount(); ++i) {
        BlockedWindowEntry entry;
        entry.enabled = ui->blockedTable->item(i, 0)->checkState() == Qt::Checked;
        entry.comment = ui->blockedTable->item(i, 1)->text();
        entry.title = ui->blockedTable->item(i, 2)->text();
        entry.className = ui->blockedTable->item(i, 3)->text();
        entry.processName = ui->blockedTable->item(i, 4)->text();
        entry.processPath = ui->blockedTable->item(i, 5)->text();
        blocked.append(entry);
    }
    cfg().setBlockedWindows(blocked);

    // hotkey bindings
    applyHotkeyBindings();

    cfg().sync();
}

void SettingsDialog::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
        buildHotkeyPage();
        loadHotkeyBindings();
    }
    QDialog::changeEvent(event);
}


