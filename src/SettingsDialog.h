#ifndef WIN_SWITCHER_SETTINGSDIALOG_H
#define WIN_SWITCHER_SETTINGSDIALOG_H

#include <QDialog>
#include <QEvent>
#include <QHash>
#include <QList>
#include <QMap>
#include <QLabel>
#include <QPushButton>

class ConfigManager;
class HotkeyPageManager;
class BlockedWindowManager;
class QListWidget;
class QListWidgetItem;
class QResizeEvent;

QT_BEGIN_NAMESPACE
namespace Ui {
    class SettingsDialog;
}
QT_END_NAMESPACE

struct SettingsSearchEntry {
    QWidget* widget = nullptr;
    int pageIndex = 0;
    QString text;
};

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(ConfigManager* config, QWidget* parent = nullptr);
    ~SettingsDialog() override;

protected:
    void changeEvent(QEvent* event) override;
    void reject() override;
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void retranslateUi();
    void filterPages(const QString& text);
    void loadSettings();
    void applySettings();
    void refreshCacheSize();
    void refreshLogSize();
    void cleanLogFiles();

    void rebuildSearchIndex();
    void collectSearchTexts(QWidget* page, int pageIndex);
    void positionSearchResults();
    void activateSearchResult(QListWidgetItem* item);
    void moveSearchSelection(bool down);
    void scrollToWidget(QWidget* widget);
    void setWidgetHighlighted(QWidget* widget, bool highlighted);
    void clearHighlights();
    void applySearchResultsTheme();

    ConfigManager* m_config;
    Ui::SettingsDialog* ui;
    HotkeyPageManager* m_hotkeyMgr;
    BlockedWindowManager* m_blockedMgr;
    bool m_loadingSettings = false;

    QPushButton* m_btnEditBlocked = nullptr;
    QPushButton* m_btnExportBlocked = nullptr;
    QPushButton* m_btnImportBlocked = nullptr;

    QListWidget* m_searchResultsList = nullptr;
    QList<SettingsSearchEntry> m_searchEntries;
    QHash<QWidget*, QString> m_originalStyles;
};

#endif
