#include "SelectionController.h"
#include "WindowGroupModel.h"
#include "WindowManager.h"
#include "GroupWindowCycler.h"
#include "utils/Util.h"
#include "core/ConfigManager.h"
#include "core/ThemeManager.h"
#include "hook/WheelEventProcessor.h"
#include <QWheelEvent>
#include <QApplication>

SelectionController::SelectionController(QListView* listView, WindowGroupModel* model,
                                         WindowManager* wm, GroupWindowCycler* cyc, QObject* parent)
    : QObject(parent), m_lv(listView), m_model(model), m_wm(wm), m_cyc(cyc) {
    m_wheelProcessor = new WheelEventProcessor(this);
    connect(m_wheelProcessor, &WheelEventProcessor::foregroundChanged,
            this, &SelectionController::foregroundChanged);
    connect(m_wheelProcessor, &WheelEventProcessor::labelTextChanged,
            this, &SelectionController::labelTextChanged);
}

void SelectionController::setLabelWidget(QWidget* label) {
    m_labelWidget = label;
}

void SelectionController::showLabelForItem(const QModelIndex& index) {
    if (!index.isValid()) return;

    QString text;
    if (m_isInGroupWindowMode) {
        auto& group = m_model->groupAt(index.row());
        if (!group.windows.isEmpty())
            text = group.windows.first().title;
    }
    if (text.isNull()) {
        auto path = m_model->groupAt(index.row()).exePath;
        text = Util::getFileDescription(path);
    }
    emit labelTextChanged(text);
}

int SelectionController::currentRow() const {
    return m_lv->currentIndex().row();
}

void SelectionController::handleOverlayAction(HotkeyAction action, Qt::KeyboardModifiers modifiers,
                                              InputSource source) {
    qInfo() << "[Action] overlayAction=" << hotkeyActionName(action)
            << "source=" << (source == InputSource::Hook ? "Hook" : "QtEvent")
            << "groupMode=" << m_isInGroupWindowMode;

    switch (action) {
    case HotkeyAction::CycleForward: {
        if (m_isInGroupWindowMode && (modifiers & Qt::AltModifier)) {
            int savedIndex = m_backupGroupIndex;
            bool shiftPressed = (modifiers & Qt::ShiftModifier);
            exitGroupWindowMode(false);
            int count = m_model->groupCount();
            if (count <= 0) return;
            int nextIndex = shiftPressed
                ? (savedIndex - 1 + count) % count
                : (savedIndex + 1) % count;
            m_lv->setCurrentIndex(m_model->index(nextIndex));
            showLabelForItem(m_model->index(nextIndex));
            return;
        }
        auto i = m_lv->currentIndex().row();
        bool shiftPressed = (modifiers & Qt::ShiftModifier);
        auto count = m_model->groupCount();
        if (count <= 0) return;
        auto nextIndex = (i - (2 * (int)shiftPressed - 1) + count) % count;
        m_lv->setCurrentIndex(m_model->index(nextIndex));
        if (!(modifiers & Qt::AltModifier))
            showLabelForItem(m_model->index(nextIndex));
        return;
    }
    case HotkeyAction::CycleBackward: {
        auto i = m_lv->currentIndex().row();
        auto count = m_model->groupCount();
        if (count <= 0) return;
        auto nextIndex = (i - 1 + count) % count;
        m_lv->setCurrentIndex(m_model->index(nextIndex));
        showLabelForItem(m_model->index(nextIndex));
        return;
    }
    case HotkeyAction::EnterGroupMode: {
        if (m_isInGroupWindowMode) {
            auto count = m_model->groupCount();
            if (count <= 0) return;
            auto next = (m_lv->currentIndex().row() + 1) % count;
            m_lv->setCurrentIndex(m_model->index(next));
        } else {
            enterGroupWindowMode();
        }
        return;
    }
    case HotkeyAction::MoveSelectionUp: {
        if (auto index = m_lv->currentIndex(); index.isValid()) {
            auto center = m_lv->visualRect(index).center();
            auto wheelEvent = new QWheelEvent(center, m_lv->mapToGlobal(center), {},
                                              {120, 0},
                                              Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
            QApplication::postEvent(m_lv, wheelEvent);
        }
        return;
    }
    case HotkeyAction::MoveSelectionDown: {
        if (auto index = m_lv->currentIndex(); index.isValid()) {
            auto center = m_lv->visualRect(index).center();
            auto wheelEvent = new QWheelEvent(center, m_lv->mapToGlobal(center), {},
                                              {-120, 0},
                                              Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
            QApplication::postEvent(m_lv, wheelEvent);
        }
        return;
    }
    case HotkeyAction::ActivateSelected: {
        emit activateAndHide();
        return;
    }
    case HotkeyAction::DismissSwitcher: {
        emit dismiss();
        return;
    }
    default:
        return;
    }
}

bool SelectionController::handleLetterJump(QChar pressedLetter) {
    if (!cfg().getLetterJumpEnabled()) return false;

    QList<int> matchingIndices;
    for (int i = 0; i < m_model->groupCount(); ++i) {
        const auto& group = m_model->groupAt(i);
        QString appName = Util::getFileDescription(group.exePath);
        if (!appName.isEmpty() && Util::getDisplayFirstLetter(appName) == pressedLetter)
            matchingIndices.append(i);
    }

    if (matchingIndices.isEmpty()) return false;

    int currentPos = matchingIndices.indexOf(m_jumpLastIndex);
    int newPos = (pressedLetter == m_jumpLastLetter && currentPos >= 0)
                     ? (currentPos + 1) % matchingIndices.size()
                     : 0;
    int targetIndex = matchingIndices[newPos];
    m_lv->setCurrentIndex(m_model->index(targetIndex));
    showLabelForItem(m_model->index(targetIndex));
    m_jumpLastLetter = pressedLetter;
    m_jumpLastIndex = targetIndex;
    return true;
}

bool SelectionController::jumpToLetter(QChar letter) {
    if (!cfg().getLetterJumpEnabled()) return false;
    auto upper = letter.toUpper();
    if (upper < 'A' || upper > 'Z') return false;
    return handleLetterJump(upper);
}

bool SelectionController::handleEventFilter(QObject* watched, QEvent* event, bool stayOpenMode) {
    if (watched != m_lv) return false;

    if (event->type() == QEvent::KeyPress && stayOpenMode) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        auto key = keyEvent->key();
        auto modifiers = keyEvent->modifiers();

        if (key == Qt::Key_Return || key == Qt::Key_Enter) {
            emit activateAndHide();
            return true;
        }
        if (key == Qt::Key_Escape) {
            emit dismiss();
            return true;
        }
        if (key == Qt::Key_Tab || key == Qt::Key_Backtab) {
            bool isShiftPressed = (modifiers & Qt::ShiftModifier);
            if (isShiftPressed)
                handleOverlayAction(HotkeyAction::CycleBackward, modifiers);
            else
                handleOverlayAction(HotkeyAction::CycleForward, modifiers);
            return true;
        }
        if (key >= Qt::Key_A && key <= Qt::Key_Z) {
            return jumpToLetter(QChar(key));
        }
    }

    if (event->type() == QEvent::Wheel) {
        auto* wheelEvent = static_cast<QWheelEvent*>(event);
        return m_wheelProcessor->handleWheelEvent(wheelEvent, m_lv, m_model, m_wm, m_cyc);
    }
    return false;
}

void SelectionController::enterGroupWindowMode() {
    auto index = m_lv->currentIndex();
    if (!index.isValid()) {
        qDebug() << "[GroupMode] enterGroupWindowMode called but currentIndex invalid";
        return;
    }

    auto group = m_model->groupAt(index.row());
    if (group.windows.size() <= 1) {
        qDebug() << "[GroupMode] enterGroupWindowMode called but only 1 window";
        return;
    }

    m_backupGroupList = m_model->groups();
    m_backupGroupIndex = index.row();
    qInfo() << "[GroupMode] Enter, backupIndex=" << m_backupGroupIndex
            << "windows=" << group.windows.size();

    QList<WindowGroup> filtered;
    for (const auto& win : group.windows) {
        WindowGroup g;
        g.exePath = group.exePath;
        g.icon = group.icon;
        g.addWindow(win);
        filtered.append(g);
    }

    m_model->setGroups(filtered);
    m_isInGroupWindowMode = true;

    emit geometryNeedsRecalc();
    m_lv->setCurrentIndex(m_model->index(0));
    showLabelForItem(m_model->index(0));
    qInfo() << "(Alt+`)Window focus:" << group.exePath << filtered.size() << "windows";
}

void SelectionController::exitGroupWindowMode(bool activateSelected) {
    if (!m_isInGroupWindowMode) {
        qDebug() << "[GroupMode] exitGroupWindowMode called but NOT in group mode";
        return;
    }
    qInfo() << "[GroupMode] exitGroupWindowMode activateSelected=" << activateSelected
            << "backupIndex=" << m_backupGroupIndex
            << "backupCount=" << m_backupGroupList.size();
    m_isInGroupWindowMode = false;

    if (activateSelected) {
        if (auto index = m_lv->currentIndex(); index.isValid()) {
            if (auto group = m_model->groupAt(index.row()); !group.windows.empty()) {
                qInfo() << "[GroupMode] activating window on exit";
                emit switchToWindowRequested(group.windows.first().hwnd,
                                              group.exePath,
                                              group.windows.first().title);
            }
        }
    }

    m_model->setGroups(m_backupGroupList);
    m_backupGroupList.clear();

    emit geometryNeedsRecalc();
    int restoreIndex = qMin(m_backupGroupIndex, m_model->groupCount() - 1);
    m_lv->setCurrentIndex(m_model->index(restoreIndex));
    showLabelForItem(m_model->index(restoreIndex));
    m_backupGroupIndex = 0;

    qInfo() << "[GroupMode] Exit, restored to index" << restoreIndex;
}

bool SelectionController::tryEnterGroupForWindow(HWND hwnd) {
    qInfo() << "[AutoGroup] tryEnterGroupForWindow hwnd=" << Qt::hex << hwnd
             << "groupCount=" << m_model->groupCount()
             << "inGroupMode=" << m_isInGroupWindowMode;
    if (m_isInGroupWindowMode) return false;

    for (int i = 0; i < m_model->groupCount(); ++i) {
        auto& group = m_model->groupAt(i);
        for (auto& w : group.windows) {
            if (w.hwnd == hwnd) {
                qInfo() << "[AutoGroup] found window in group" << i
                         << "windows=" << group.windows.size();
                if (group.windows.size() <= 1) {
                    qInfo() << "[AutoGroup] single window group, skip";
                    return false;
                }
                qInfo() << "[AutoGroup] entering group mode for" << group.windows.size()
                         << "windows";
                m_lv->setCurrentIndex(m_model->index(i));
                enterGroupWindowMode();
                return true;
            }
        }
    }
    qInfo() << "[AutoGroup] window not found in any group";
    return false;
}

void SelectionController::resetState() {
    m_jumpLastLetter = {};
    m_jumpLastIndex = -1;
    m_wheelProcessor->reset();
}

void SelectionController::resetAll() {
    resetState();
    m_isInGroupWindowMode = false;
    m_backupGroupList.clear();
    m_backupGroupIndex = 0;
}

void SelectionController::handleListItemClicked(const QModelIndex& index, bool stayOpenMode) {
    qInfo() << "[Click] item row=" << index.row() << "groupMode=" << m_isInGroupWindowMode
             << "stayOpen=" << stayOpenMode << "mouseClickActivate=" << cfg().getMouseClickActivateEnabled();

    if (!cfg().getMouseClickActivateEnabled() && !stayOpenMode) {
        qDebug() << "[Click] ignored (mouseClickActivate disabled, not stay-open)";
        return;
    }

    if (m_isInGroupWindowMode) {
        qInfo() << "[Click] in group mode -> exitGroupWindowMode(true)";
        exitGroupWindowMode(true);
        return;
    }

    auto& group = m_model->groupAt(index.row());
    qDebug() << "[Click] group windows=" << group.windows.size()
             << "clickShowGroup=" << cfg().getClickShowGroupForMultiWindow();
    if (group.windows.size() > 1 && cfg().getClickShowGroupForMultiWindow()) {
        qInfo() << "[Click] enterGroupWindowMode";
        enterGroupWindowMode();
    } else {
        if (!group.windows.empty()) {
            qInfo() << "[Click] switchToWindow + hide";
            emit switchToWindowRequested(group.windows.first().hwnd,
                                         group.exePath,
                                         group.windows.first().title);
        }
        emit dismiss();
    }
}
