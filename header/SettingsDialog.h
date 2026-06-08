#ifndef WIN_SWITCHER_SETTINGSDIALOG_H
#define WIN_SWITCHER_SETTINGSDIALOG_H

#include <QDialog>
#include <QEvent>
#include <QMap>
#include <QPushButton>
#include "utils/HotkeyAction.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class SettingsDialog;
}
QT_END_NAMESPACE

class HotkeyRecorder;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);
    ~SettingsDialog() override;

protected:
    void changeEvent(QEvent* event) override;

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

    Ui::SettingsDialog* ui;
    bool m_loadingSettings = false;

    QMap<HotkeyAction, HotkeyRecorder*> m_recorders;

    // Programmatically created buttons for Blocked Windows page
    QPushButton* m_btnEditBlocked = nullptr;
    QPushButton* m_btnExportBlocked = nullptr;
    QPushButton* m_btnImportBlocked = nullptr;
};

#endif
