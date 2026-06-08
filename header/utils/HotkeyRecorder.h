#ifndef WIN_SWITCHER_HOTKEYRECORDER_H
#define WIN_SWITCHER_HOTKEYRECORDER_H

#include <QWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QKeyEvent>
#include "utils/HotkeyAction.h"

class HotkeyRecorder : public QWidget {
    Q_OBJECT

public:
    explicit HotkeyRecorder(HotkeyAction action, QWidget* parent = nullptr);

    void setBindings(const QList<HotkeyBinding>& bindings);
    QList<HotkeyBinding> bindings() const { return m_bindings; }

    HotkeyAction action() const { return m_action; }

signals:
    void bindingsChanged(HotkeyAction action, const QList<HotkeyBinding>& bindings);

private slots:
    void onAddClicked();
    void onBindingClicked(int index);
    void onRemoveClicked(int index);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void rebuildUi();
    void startRecording(int index);
    void finishRecording(const HotkeyBinding& binding);
    void cancelRecording();

    HotkeyAction m_action;
    QList<HotkeyBinding> m_bindings;
    QHBoxLayout* m_layout;
    QLabel* m_label;

    int m_recordingIndex = -1; // -1 = not recording
};

#endif //WIN_SWITCHER_HOTKEYRECORDER_H
