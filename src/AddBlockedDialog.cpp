#include "AddBlockedDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QLabel>

AddBlockedDialog::AddBlockedDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Add Blocked Rule"));
    setMinimumWidth(420);

    auto* mainLayout = new QVBoxLayout(this);

    auto* form = new QFormLayout;
    m_titleEdit = new QLineEdit;
    m_titleEdit->setPlaceholderText(tr("Leave empty to match any title"));
    form->addRow(tr("Title:"), m_titleEdit);

    m_classNameEdit = new QLineEdit;
    m_classNameEdit->setPlaceholderText(tr("Leave empty to match any class"));
    form->addRow(tr("Class Name:"), m_classNameEdit);

    m_processNameEdit = new QLineEdit;
    m_processNameEdit->setPlaceholderText(tr("e.g. chrome.exe"));
    form->addRow(tr("Process Name:"), m_processNameEdit);

    m_processPathEdit = new QLineEdit;
    m_processPathEdit->setPlaceholderText(tr("e.g. C:/Program Files/Chrome"));
    form->addRow(tr("Process Path:"), m_processPathEdit);

    m_commentEdit = new QLineEdit;
    m_commentEdit->setPlaceholderText(tr("Optional description"));
    form->addRow(tr("Comment:"), m_commentEdit);

    mainLayout->addLayout(form);

    m_enabledCheck = new QCheckBox(tr("Enabled"));
    m_enabledCheck->setChecked(true);
    mainLayout->addWidget(m_enabledCheck);

    auto* btnLayout = new QHBoxLayout;
    m_fromCurrentBtn = new QPushButton(tr("From Current Window"));
    btnLayout->addWidget(m_fromCurrentBtn);
    btnLayout->addStretch();
    mainLayout->addLayout(btnLayout);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    mainLayout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        if (m_titleEdit->text().trimmed().isEmpty() &&
            m_classNameEdit->text().trimmed().isEmpty() &&
            m_processNameEdit->text().trimmed().isEmpty() &&
            m_processPathEdit->text().trimmed().isEmpty()) {
            // Allow saving with empty fields? The rule just won't match anything.
            // We should warn the user.
            return;
        }
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_fromCurrentBtn, &QPushButton::clicked, this, &AddBlockedDialog::fromCurrentRequested);
}

void AddBlockedDialog::setFields(const BlockedWindowEntry& entry) {
    m_titleEdit->setText(entry.title);
    m_classNameEdit->setText(entry.className);
    m_processNameEdit->setText(entry.processName);
    m_processPathEdit->setText(entry.processPath);
    m_commentEdit->setText(entry.comment);
    m_enabledCheck->setChecked(entry.enabled);
}

BlockedWindowEntry AddBlockedDialog::result() const {
    BlockedWindowEntry entry;
    entry.title = m_titleEdit->text().trimmed();
    entry.className = m_classNameEdit->text().trimmed();
    entry.processName = m_processNameEdit->text().trimmed();
    entry.processPath = m_processPathEdit->text().trimmed();
    entry.comment = m_commentEdit->text().trimmed();
    entry.enabled = m_enabledCheck->isChecked();
    return entry;
}
