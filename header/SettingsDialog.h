#ifndef WIN_SWITCHER_SETTINGSDIALOG_H
#define WIN_SWITCHER_SETTINGSDIALOG_H

#include <QDialog>
#include <QEvent>
#include <QMap>
#include <QLabel>
#include <QPushButton>
#include "core/HotkeyAction.h"

class ConfigManager;

QT_BEGIN_NAMESPACE
namespace Ui {
    class SettingsDialog;
}
QT_END_NAMESPACE

class HotkeyRecorder;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(ConfigManager* config, QWidget* parent = nullptr);
    ~SettingsDialog() override;

protected:
    void changeEvent(QEvent* event) override;
    void reject() override;
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;

private:
    void retranslateUi();
    void filterPages(const QString& text);
    void loadSettings();
    void applySettings();
    void applyStyleSheet();

    void buildHotkeyPage();
    void loadHotkeyBindings();
    void applyHotkeyBindings();
    bool checkConflict(HotkeyAction action, const HotkeyBinding& binding,
                       HotkeyAction& conflictAction, int& conflictIndex) const;
    void checkLetterJumpConflict();

    ConfigManager* m_config;
    Ui::SettingsDialog* ui;
    bool m_loadingSettings = false;
    bool m_resolvingConflict = false;

    QMap<HotkeyAction, HotkeyRecorder*> m_recorders;

    // Programmatically created buttons for Blocked Windows page
    QPushButton* m_btnEditBlocked = nullptr;
    QPushButton* m_btnExportBlocked = nullptr;
    QPushButton* m_btnImportBlocked = nullptr;
    QLabel* m_showSwitcherWarning = nullptr; // checks SwitchToNextWindow bindings
};

#endif
