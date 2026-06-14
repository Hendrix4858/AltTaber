#include "hook/HotkeyRecorder.h"
#include "core/HotkeyAction.h"
#include "hook/KeyboardHooker.h"
#include <QApplication>
#include <QStyle>
#include <QTimer>

static QString bindingButtonText(const HotkeyBinding& binding) {
    QString s = binding.toString();
    return s.isEmpty() ? QStringLiteral("-") : s;
}

HotkeyRecorder::HotkeyRecorder(HotkeyAction action, QWidget* parent)
    : QWidget(parent), m_action(action) {
    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(4);

    m_label = new QLabel(hotkeyActionDisplayName(action), this);
    m_label->setMinimumWidth(160);
    m_layout->addWidget(m_label);
    m_layout->addStretch();

    setFocusPolicy(Qt::StrongFocus);
    rebuildUi();
}

HotkeyRecorder::~HotkeyRecorder() {
    cancelRecording();
}

void HotkeyRecorder::setBindings(const QList<HotkeyBinding>& bindings) {
    m_bindings = bindings;
    rebuildUi();
}

void HotkeyRecorder::rebuildUi() {
    while (m_layout->count() > 2) {
        auto* item = m_layout->takeAt(m_layout->count() - 1);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    for (int i = 0; i < m_bindings.size(); ++i) {
        bool protect = isProtectedBinding(m_action, m_bindings[i]);
        QString text = bindingButtonText(m_bindings[i]);
        auto* btn = new QPushButton(protect ? text : text + QStringLiteral(" \u00D7"), this);
        btn->setFixedHeight(28);
        if (protect) {
            btn->setEnabled(false);
        } else {
            btn->setCursor(Qt::PointingHandCursor);
            btn->setProperty("bindingIndex", i);
            connect(btn, &QPushButton::clicked, this, [this, i]() {
                onRemoveClicked(i);
            });
        }
        m_layout->addWidget(btn);
    }

    auto* addBtn = new QPushButton(tr("+ Add"), this);
    addBtn->setFixedHeight(28);
    addBtn->setCursor(Qt::PointingHandCursor);
    connect(addBtn, &QPushButton::clicked, this, &HotkeyRecorder::onAddClicked);
    m_layout->addWidget(addBtn);
}

void HotkeyRecorder::onAddClicked() {
    startRecording(m_bindings.size());
}


void HotkeyRecorder::onRemoveClicked(int index) {
    if (index >= 0 && index < m_bindings.size() && !isProtectedBinding(m_action, m_bindings[index])) {
        m_bindings.removeAt(index);
        rebuildUi();
        emit bindingsChanged(m_action, m_bindings);
    }
}

void HotkeyRecorder::saveBackup() {
    m_previousBindings = m_bindings;
}

void HotkeyRecorder::rollback() {
    blockSignals(true);
    setBindings(m_previousBindings);
    blockSignals(false);
}

void HotkeyRecorder::startRecording(int index) {
    saveBackup();
    m_recordingIndex = index;

    HWND targetHwnd = reinterpret_cast<HWND>(window()->winId());
    KeyboardHooker::setRecordingTarget(targetHwnd);
    qApp->installEventFilter(this);
}

void HotkeyRecorder::finishRecording(const HotkeyBinding& binding) {
    KeyboardHooker::clearRecordingTarget();
    if (m_recordingIndex < 0) return;

    if (m_recordingIndex >= m_bindings.size()) {
        m_bindings.append(binding);
    } else if (m_recordingIndex < m_bindings.size()) {
        m_bindings[m_recordingIndex] = binding;
    }

    qApp->removeEventFilter(this);
    m_recordingIndex = -1;
    rebuildUi();
    emit bindingsChanged(m_action, m_bindings);
}

void HotkeyRecorder::cancelRecording() {
    KeyboardHooker::clearRecordingTarget();
    if (m_recordingIndex < 0) return;
    qApp->removeEventFilter(this);
    m_recordingIndex = -1;
    rebuildUi();
}

bool HotkeyRecorder::eventFilter(QObject* obj, QEvent* event) {
    // Fallback: Qt event path is deliberately minimal.
    // Only handles Escape-to-cancel. Raw key recording is done via
    // KeyboardHooker -> PostMessage -> SettingsDialog::nativeEvent.
    if (m_recordingIndex < 0) return QWidget::eventFilter(obj, event);
    if (event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Escape) {
            qDebug() << "[RecordFallback] Escape -> cancelRecording";
            cancelRecording();
            return true;
        }
        return true;
    }
    return QWidget::eventFilter(obj, event);
}
