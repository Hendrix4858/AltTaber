#include "BlockedWindowManager.h"
#include "AddBlockedDialog.h"
#include <QTableWidget>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QFileInfo>
#include <windows.h>

BlockedWindowManager::BlockedWindowManager(ConfigManager* config, QTableWidget* table,
                                           QPushButton* btnAdd, QPushButton* btnRemove,
                                           QObject* parent)
    : QObject(parent), m_config(config), m_table(table), m_btnAdd(btnAdd), m_btnRemove(btnRemove) {}

void BlockedWindowManager::loadFromConfig() {
    m_table->setRowCount(0);
    auto blocked = m_config->getBlockedWindows();
    for (const auto& entry : blocked) {
        fillRow(m_table->rowCount(), entry);
    }
}

QList<BlockedWindowEntry> BlockedWindowManager::collectEntries() const {
    QList<BlockedWindowEntry> list;
    for (int i = 0; i < m_table->rowCount(); ++i) {
        list.append(entryFromRow(i));
    }
    return list;
}

void BlockedWindowManager::addEntry(const BlockedWindowEntry& entry) {
    int row = m_table->rowCount();
    m_table->insertRow(row);
    fillRow(row, entry);
}

void BlockedWindowManager::editEntry(int row) {
    if (row < 0) return;
    auto entry = entryFromRow(row);

    AddBlockedDialog dlg;
    dlg.setFields(entry);
    if (dlg.exec() == QDialog::Accepted) {
        fillRow(row, dlg.result());
    }
}

BlockedWindowEntry BlockedWindowManager::entryFromRow(int row) const {
    BlockedWindowEntry entry;
    entry.enabled = m_table->item(row, 0)->checkState() == Qt::Checked;
    entry.comment = m_table->item(row, 1)->text();
    entry.title = m_table->item(row, 2)->text();
    entry.className = m_table->item(row, 3)->text();
    entry.processName = m_table->item(row, 4)->text();
    entry.processPath = m_table->item(row, 5)->text();
    return entry;
}

void BlockedWindowManager::fillRow(int row, const BlockedWindowEntry& entry) {
    auto* enabledItem = new QTableWidgetItem();
    enabledItem->setFlags(enabledItem->flags() | Qt::ItemIsUserCheckable);
    enabledItem->setCheckState(entry.enabled ? Qt::Checked : Qt::Unchecked);
    m_table->setItem(row, 0, enabledItem);
    m_table->setItem(row, 1, new QTableWidgetItem(entry.comment));
    m_table->setItem(row, 2, new QTableWidgetItem(entry.title));
    m_table->setItem(row, 3, new QTableWidgetItem(entry.className));
    m_table->setItem(row, 4, new QTableWidgetItem(entry.processName));
    m_table->setItem(row, 5, new QTableWidgetItem(entry.processPath));
}

QPushButton* BlockedWindowManager::createEditButton(QWidget* parent) {
    auto* btn = new QPushButton(tr("Edit"), parent);
    setupEditButton(btn);
    return btn;
}

QPushButton* BlockedWindowManager::createExportButton(QWidget* parent) {
    auto* btn = new QPushButton(tr("Export"), parent);
    setupExportButton(btn);
    return btn;
}

QPushButton* BlockedWindowManager::createImportButton(QWidget* parent) {
    auto* btn = new QPushButton(tr("Import"), parent);
    setupImportButton(btn);
    return btn;
}

void BlockedWindowManager::setupEditButton(QPushButton* btn) {
    connect(btn, &QPushButton::clicked, this, [this] {
        int row = m_table->currentRow();
        if (row < 0) return;
        editEntry(row);
    });
}

void BlockedWindowManager::setupExportButton(QPushButton* btn) {
    connect(btn, &QPushButton::clicked, this, [this] {
        QString filePath = QFileDialog::getSaveFileName(
            nullptr, tr("Export Blocked Rules"), QString(), tr("JSON Files (*.json)"));
        if (filePath.isEmpty()) return;
        QJsonArray arr;
        for (int i = 0; i < m_table->rowCount(); ++i) {
            auto entry = entryFromRow(i);
            QJsonObject obj;
            obj["enabled"] = entry.enabled;
            obj["comment"] = entry.comment;
            obj["title"] = entry.title;
            obj["class"] = entry.className;
            obj["processName"] = entry.processName;
            obj["processPath"] = entry.processPath;
            arr.append(obj);
        }
        QFile file(filePath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
            file.close();
            QMessageBox::information(nullptr, tr("Export"), tr("Exported %1 rules.").arg(arr.size()));
        }
    });
}

void BlockedWindowManager::setupImportButton(QPushButton* btn) {
    connect(btn, &QPushButton::clicked, this, [this] {
        QString filePath = QFileDialog::getOpenFileName(
            nullptr, tr("Import Blocked Rules"), QString(), tr("JSON Files (*.json)"));
        if (filePath.isEmpty()) return;
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) return;
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        file.close();
        if (!doc.isArray()) {
            QMessageBox::warning(nullptr, tr("Import"), tr("Invalid format."));
            return;
        }
        QJsonArray arr = doc.array();
        int imported = 0;
        for (const auto& val : arr) {
            QJsonObject obj = val.toObject();
            auto entry = BlockedWindowEntry{
                .enabled = obj.value("enabled").toBool(true),
                .comment = obj["comment"].toString(),
                .title = obj["title"].toString(),
                .className = obj["class"].toString(),
                .processName = obj["processName"].toString(),
                .processPath = obj["processPath"].toString()
            };
            if (entry.title.isEmpty() && entry.className.isEmpty() &&
                entry.processName.isEmpty() && entry.processPath.isEmpty())
                continue;
            addEntry(entry);
            imported++;
        }
        QMessageBox::information(nullptr, tr("Import"), tr("Imported %1 rules.").arg(imported));
    });
}
