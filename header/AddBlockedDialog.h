#ifndef WIN_SWITCHER_ADDBLOCKEDDIALOG_H
#define WIN_SWITCHER_ADDBLOCKEDDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QCheckBox>
#include "core/ConfigManager.h"

class AddBlockedDialog : public QDialog {
    Q_OBJECT

public:
    explicit AddBlockedDialog(QWidget* parent = nullptr);

    void setFields(const BlockedWindowEntry& entry);
    BlockedWindowEntry result() const;

private:
    QLineEdit* m_titleEdit;
    QLineEdit* m_classNameEdit;
    QLineEdit* m_processNameEdit;
    QLineEdit* m_processPathEdit;
    QLineEdit* m_commentEdit;
    QCheckBox* m_enabledCheck;
};

#endif //WIN_SWITCHER_ADDBLOCKEDDIALOG_H
