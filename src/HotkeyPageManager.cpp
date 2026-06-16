#include "HotkeyPageManager.h"
#include "core/ConfigManager.h"
#include "core/ThemeManager.h"
#include "hook/HotkeyRecorder.h"
#include "hook/KeyboardHooker.h"
#include <QStackedWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QDebug>

HotkeyPageManager::HotkeyPageManager(ConfigManager* config, QObject* parent)
    : QObject(parent), m_config(config) {}

void HotkeyPageManager::buildHotkeyPage(QStackedWidget* stackedWidget, QLabel* hotkeyPlaceholder) {
    auto* hotkeyPage = stackedWidget->widget(2);

    for (auto it = m_recorders.begin(); it != m_recorders.end(); ++it) {
        if (auto* r = it.value()) {
            r->disconnect();
            r->setParent(nullptr);
            r->deleteLater();
        }
    }
    m_recorders.clear();

    auto* hotkeyLayout = hotkeyPage->layout();
    if (hotkeyLayout) {
        QLayoutItem* item;
        while ((item = hotkeyLayout->takeAt(0)) != nullptr) {
            if (auto* w = item->widget()) {
                if (w == hotkeyPlaceholder) {
                    w->hide();
                } else {
                    w->setParent(nullptr);
                    w->deleteLater();
                }
            }
            delete item;
        }
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

    static const HotkeyAction displayOrder[] = {
        HotkeyAction::SwitchToNextWindow,
        HotkeyAction::SwitchToPreviousWindow,
        HotkeyAction::CycleProcessWindows,
        HotkeyAction::SwitchProcessWindow,
        HotkeyAction::TogglePause,
        HotkeyAction::ShowSwitcherStayOpen,
        HotkeyAction::ExpandGroup,
        HotkeyAction::CycleForward,
        HotkeyAction::CycleBackward,
        HotkeyAction::MoveSelectionUp,
        HotkeyAction::MoveSelectionDown,
        HotkeyAction::ActivateSelected,
        HotkeyAction::DismissSwitcher,
    };

    auto makeScopeHeader = [&](const QString& text) -> QLabel* {
        auto* header = new QLabel(text, scrollContent);
        header->setStyleSheet(
            QString("color: %1; font-weight: bold; font-size: 13px; padding: 8px 0 2px 0;")
                .arg(c.accentColor.name()));
        return header;
    };

    bool globalHeaderAdded = false;
    bool overlayHeaderAdded = false;
    for (auto action : displayOrder) {
        auto scope = hotkeyActionScope(action);
        if (scope == HotkeyScope::Global && !globalHeaderAdded) {
            contentLayout->addWidget(makeScopeHeader(tr("Global Hotkeys (always active)")));
            globalHeaderAdded = true;
        }
        if (scope == HotkeyScope::Overlay && !overlayHeaderAdded) {
            contentLayout->addWidget(makeScopeHeader(tr("Overlay Hotkeys (only when overlay is visible)")));
            overlayHeaderAdded = true;
        }

        auto* recorder = new HotkeyRecorder(action, scrollContent);
        m_recorders[action] = recorder;
        contentLayout->addWidget(recorder);

        connect(recorder, &HotkeyRecorder::bindingsChanged, this,
                [this, action, recorder](HotkeyAction, const QList<HotkeyBinding>& bindings) {
            if (m_resolvingConflict) {
                checkLetterJumpConflict();
                return;
            }
            m_resolvingConflict = true;

            HotkeyAction conflictAction;
            int conflictIndex;
            for (const auto& b : bindings) {
                if (checkConflict(action, b, conflictAction, conflictIndex)) {
                    auto result = QMessageBox::warning(nullptr, tr("Conflict"),
                        tr("Hotkey \"%1\" is already used by \"%2\".\nOverwrite?")
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

    m_showSwitcherWarning = new QLabel(
        tr("Warning: Switch to Next Window has no hotkey assigned, AltTaber cannot be activated"),
        scrollContent);
    m_showSwitcherWarning->setStyleSheet("color: red; font-weight: bold; padding: 4px 0;");
    m_showSwitcherWarning->setVisible(false);
    contentLayout->addWidget(m_showSwitcherWarning);

    contentLayout->addSpacing(12);
    auto* resetBtn = new QPushButton(tr("Reset to Defaults"), scrollContent);
    connect(resetBtn, &QPushButton::clicked, this, [this]() {
        auto reply = QMessageBox::question(nullptr, tr("Reset Hotkeys"),
                                            tr("Reset all hotkeys to their default values?"),
                                            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            for (auto it = m_recorders.begin(); it != m_recorders.end(); ++it)
                it.value()->setBindings({});
            m_config->resetHotkeys();
            loadBindings();
        }
    });
    contentLayout->addWidget(resetBtn);
    contentLayout->addStretch();

    scrollArea->setWidget(scrollContent);

    if (!hotkeyLayout) {
        hotkeyLayout = new QVBoxLayout(hotkeyPage);
    }
    hotkeyLayout->setContentsMargins(0, 0, 0, 0);
    hotkeyLayout->addWidget(scrollArea);
}

void HotkeyPageManager::loadBindings() {
    auto bindings = m_config->getHotkeyBindings();
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

void HotkeyPageManager::applyBindings() {
    HotkeyBindings bindings;
    for (auto it = m_recorders.begin(); it != m_recorders.end(); ++it) {
        bindings[it.key()] = it.value()->bindings();
    }
    m_config->setHotkeyBindings(bindings);
}

void HotkeyPageManager::cancelAllRecordings() {
    for (auto it = m_recorders.begin(); it != m_recorders.end(); ++it)
        it.value()->cancelRecording();
}

bool HotkeyPageManager::handleRecordedKey(quint32 vk, quint32 scan, DWORD flags,
                                           Qt::KeyboardModifiers mods) {
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

bool HotkeyPageManager::checkConflict(HotkeyAction action, const HotkeyBinding& binding,
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

void HotkeyPageManager::checkLetterJumpConflict() {
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
    emit bindingsChanged();
}
