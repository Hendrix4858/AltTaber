#ifndef WIN_SWITCHER_SETTINGSDIALOG_H
#define WIN_SWITCHER_SETTINGSDIALOG_H

#include <QDialog>
#include <QEvent>

QT_BEGIN_NAMESPACE
namespace Ui {
    class SettingsDialog;
}
QT_END_NAMESPACE

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

    Ui::SettingsDialog* ui;
    bool m_loadingSettings = false;
};

#endif
