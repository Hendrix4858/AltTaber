#ifndef WIN_SWITCHER_BLOCKEDWINDOWMANAGER_H
#define WIN_SWITCHER_BLOCKEDWINDOWMANAGER_H

#include <QObject>
#include <QList>
#include "core/ConfigManager.h"

class QTableWidget;
class QPushButton;

class BlockedWindowManager : public QObject {
    Q_OBJECT

public:
    explicit BlockedWindowManager(ConfigManager* config, QTableWidget* table,
                                  QPushButton* btnAdd, QPushButton* btnRemove,
                                  QObject* parent = nullptr);

    void loadFromConfig();
    QList<BlockedWindowEntry> collectEntries() const;

    QPushButton* createEditButton(QWidget* parent);
    QPushButton* createExportButton(QWidget* parent);
    QPushButton* createImportButton(QWidget* parent);

    void setupEditButton(QPushButton* btn);
    void setupExportButton(QPushButton* btn);
    void setupImportButton(QPushButton* btn);

private slots:
    void addFromDialog();
    void removeSelected();

private:
    void addEntry(const BlockedWindowEntry& entry);
    void editEntry(int row);
    BlockedWindowEntry entryFromRow(int row) const;
    void fillRow(int row, const BlockedWindowEntry& entry);

    ConfigManager* m_config;
    QTableWidget* m_table;
    QPushButton* m_btnAdd;
    QPushButton* m_btnRemove;
};

#endif //WIN_SWITCHER_BLOCKEDWINDOWMANAGER_H
