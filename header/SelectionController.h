#ifndef WIN_SWITCHER_SELECTIONCONTROLLER_H
#define WIN_SWITCHER_SELECTIONCONTROLLER_H

#include <QObject>
#include <QListView>
#include <QKeyEvent>
#include "WindowTypes.h"
#include "core/HotkeyAction.h"

class WindowGroupModel;
class WindowManager;
class GroupWindowCycler;
class WheelEventProcessor;

enum class InputSource {
    KeyboardInput,
    LowLevelHook
};

class SelectionController : public QObject {
    Q_OBJECT

public:
    explicit SelectionController(QListView* listView, WindowGroupModel* model,
                                 WindowManager* wm, GroupWindowCycler* cyc, QObject* parent = nullptr);

    bool handleEventFilter(QObject* watched, QEvent* event, bool stayOpenMode);
    bool jumpToLetter(QChar letter);

    void handleOverlayAction(HotkeyAction action, Qt::KeyboardModifiers modifiers,
                             InputSource source = InputSource::KeyboardInput);
    void handleListItemClicked(const QModelIndex& index, bool stayOpenMode);

    void showLabelForItem(const QModelIndex& index);
    void setLabelWidget(QWidget* label);

    bool isInGroupMode() const { return m_isGroupExpanded; }
    int currentRow() const;

    void collapseGroup(bool activateSelected);

    bool tryEnterGroupForWindow(HWND hwnd);

    void resetState();
    void resetAll();

signals:
    void labelTextChanged(const QString& text);
    void geometryNeedsRecalc();
    void activateAndHide();
    void dismiss();
    void switchToWindowRequested(HWND hwnd, const QString& exePath, const QString& title);
    void foregroundChanged(HWND hwnd);

private:
    void expandGroup();
    void cycleSelection(int direction); // +1 forward, -1 backward
    bool handleLetterJump(QChar pressedLetter);

    QListView* m_listView;
    WindowGroupModel* m_model;
    WindowManager* m_windowManager;
    GroupWindowCycler* m_groupCycler;
    QWidget* m_labelWidget = nullptr;

    // Letter jump state
    QChar m_jumpLastLetter;
    int m_jumpLastIndex = -1;

    // Group mode state
    bool m_isGroupExpanded = false;
    QList<WindowGroup> m_expandedGroupBackup;
    int m_expandedGroupBackupIndex = 0;

    WheelEventProcessor* m_wheelProcessor = nullptr;
};

#endif //WIN_SWITCHER_SELECTIONCONTROLLER_H
