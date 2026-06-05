#ifndef WIN_SWITCHER_SETTINGSDIALOG_H
#define WIN_SWITCHER_SETTINGSDIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QStackedWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);
    ~SettingsDialog() override;

protected:
    void changeEvent(QEvent* event) override;

private:
    void setupUi();
    void retranslateUi();
    void setupGeneralPage();
    void setupDisplayPage();
    void setupHotkeyPage();
    void setupAboutPage();
    void filterPages(const QString& text);
    void loadSettings();
    void applySettings();

    QLineEdit* m_searchEdit;
    QListWidget* m_navList;
    QStackedWidget* m_stackedWidget;
    QPushButton* m_btnOk;
    QPushButton* m_btnCancel;
    QPushButton* m_btnApply;

    QComboBox* m_langCombo;
    QCheckBox* m_startupCheck;
    QComboBox* m_monitorCombo;
    QLabel* m_aboutDesc;
    QLabel* m_titleLabel;
    QWidget* m_generalPage;
    QWidget* m_displayPage;
    QWidget* m_hotkeyPage;
    QWidget* m_aboutPage;

    struct NavItem {
        int pageIndex;
    };
    QList<NavItem> m_navItems;
    bool m_loadingSettings = false;
};

#endif
