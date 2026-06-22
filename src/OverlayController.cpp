#include "OverlayController.h"
#include "OverlayView.h"
#include "WindowGroupModel.h"
#include "WindowManager.h"
#include "utils/Util.h"
#include "core/ConfigManager.h"
#include "lifecycle/SystemTray.h"
#include "core/ThemeManager.h"
#include <QDebug>
#include <QWindow>
#include <QScreen>
#include <QTimer>
#include <QGuiApplication>
#include <QListView>
#include <QElapsedTimer>

OverlayController::OverlayController(OverlayView& view, WindowGroupModel* model, WindowManager* wm,
                                     QObject* parent)
    : QObject(parent), m_view(view), m_model(model), m_windowManager(wm) {}

void OverlayController::setOverlayBindings(const HotkeyBindings& bindings) {
    m_overlayBindings.clear();
    for (auto it = bindings.begin(); it != bindings.end(); ++it) {
        if (hotkeyActionScope(it.key()) == HotkeyScope::Overlay)
            m_overlayBindings[it.key()] = it.value();
    }
}

void OverlayController::handleGlobalAction(HotkeyAction action, Qt::KeyboardModifiers modifiers) {
    qInfo() << "[OverlayCtrl] handleGlobalAction" << hotkeyActionName(action)
            << "state=" << (int)m_overlayState;

    switch (action) {
    case HotkeyAction::SwitchToNextWindow:
        if (m_overlayState == OverlayState::Hidden) {
            handleIntent(OverlayIntent::ShowSwitcher);
        } else if (m_overlayState == OverlayState::Visible) {
            emit actionForwarded(HotkeyAction::CycleForward, modifiers);
        }
        return;

    case HotkeyAction::SwitchToPreviousWindow:
        if (m_overlayState == OverlayState::Hidden) {
            handleIntent(OverlayIntent::ShowSwitcherBackward);
        } else if (m_overlayState == OverlayState::Visible) {
            emit actionForwarded(HotkeyAction::CycleBackward, modifiers);
        }
        return;

    case HotkeyAction::ShowSwitcherStayOpen:
        if (m_overlayState == OverlayState::Hidden) {
            handleIntent(OverlayIntent::ShowSwitcher);
        } else if (m_overlayState == OverlayState::Visible) {
            m_stayOpenMode = true;
            emit actionForwarded(HotkeyAction::CycleForward, modifiers);
        }
        setStayOpenMode(true);
        return;

    default:
        break;
    }
}

bool OverlayController::forceShow() {
    static int s_showCount = 0;
    ++s_showCount;
    HWND hwnd = m_view.hWnd();
    qInfo() << "[Show] forceShow (via view) showCount=" << s_showCount;
    m_view.showOverlay();
    return true;
}

QRect OverlayController::calculateGeometry(QScreen* screen) {
    if (!screen) {
        auto* widget = qobject_cast<QWidget*>(QObject::parent());
        if (widget) screen = widget->screen();
        if (!screen) screen = QGuiApplication::primaryScreen();
    }
    if (!screen) return {};

    auto screenGeo = screen->geometry();
    int maxWidth = screenGeo.width() - 24 - 24;

    int totalNeeded = m_model->groupCount() * 80;
    int iconSize = 64;
    int gridWidth = 80;
    int minIcon = cfg().getMinIconSize();

    if (totalNeeded > maxWidth) {
        gridWidth = maxWidth / m_model->groupCount();
        iconSize = qMax(minIcon, gridWidth - 16);
        if (iconSize > gridWidth)
            iconSize = gridWidth;
    }

    m_view.applyIconLayout(iconSize, gridWidth, gridWidth > 80 ? 80 : gridWidth);

    int listWidth = gridWidth * m_model->groupCount();
    auto listViewRect = QRect(0, 0, listWidth, qMax(gridWidth, 80));
    auto widgetRect = listViewRect.marginsAdded({24, 24, 24, 24});
    widgetRect.moveCenter(screenGeo.center());
    listViewRect.moveCenter(widgetRect.center());

    QPoint listViewOffset = listViewRect.topLeft() - widgetRect.topLeft();

    OverlayLayout layout;
    layout.widgetRect = widgetRect;
    layout.listRect = QRect(listViewOffset, listViewRect.size());
    m_view.applyLayout(layout);

    return widgetRect;
}

int OverlayController::calculateInitialIndex() const {
    int count = m_model->groupCount();
    if (count == 0) return -1;

    if (m_wasInvokedBackward && count >= 2)
        return count - 1;

    if (count >= 2) {
        HWND fg = GetForegroundWindow();
        auto& firstGroup = m_model->groupAt(0);
        for (auto& w : firstGroup.windows) {
            if (w.hwnd == fg) return 1;
        }
    }
    return 0;
}

void OverlayController::reconcileState() {
    // State reconciliation without direct widget queries
    // m_overlayState is the source of truth
}

void OverlayController::handleIntent(OverlayIntent intent) {
    reconcileState();
    qDebug() << "[OverlayCtrl] handleIntent intent=" << (int)intent
             << "state=" << (int)m_overlayState;
    transition(intent);
}

void OverlayController::transition(OverlayIntent intent) {
    qDebug() << "[Transition] state=" << (int)m_overlayState
             << "intent=" << (int)intent
             << "stayOpen=" << m_stayOpenMode
             << "listDirty=" << m_listDirty;

    switch (m_overlayState) {

    case OverlayState::Hidden:
        if (intent == OverlayIntent::ShowSwitcher || intent == OverlayIntent::FallbackShow ||
            intent == OverlayIntent::ShowSwitcherBackward) {
            m_wasInvokedBackward = (intent == OverlayIntent::ShowSwitcherBackward);
            qDebug() << "[Transition] Hidden + Show = show (backward="
                     << m_wasInvokedBackward << ")";
            showWindow();
        } else {
            qDebug() << "[Transition] Hidden +" << (int)intent << "\u2192 no-op";
        }
        break;

    case OverlayState::Visible:
        if (intent == OverlayIntent::SessionEndConditionMet) {
            if (m_sessionInfo.endTrigger == SessionEndTrigger::ModifierRelease && !m_stayOpenMode) {
                qInfo() << "[Transition] ModifierRelease → emit sessionFinished + hide";
                emit sessionFinished();
                hideWindow();
            } else {
                qInfo() << "[Transition] SessionEndConditionMet → no-op (stayOpen or explicit action)";
            }
        } else if (intent == OverlayIntent::Dismiss) {
            qInfo() << "[Transition] Dismiss → hide";
            hideWindow();
        } else {
            qDebug() << "[Transition] Visible +" << (int)intent << "→ no-op";
        }
        break;

    default:
        qDebug() << "[Transition] state=" << (int)m_overlayState
                 << "intent=" << (int)intent << "→ no-op (unused state)";
        break;
    }
}

void OverlayController::showWindow() {
    qInfo() << "[OverlayCtrl] showWindow state=" << (int)m_overlayState;

    if (m_listDirty) {
        qDebug() << "[OverlayCtrl] list dirty, refreshing...";
        if (!refreshWindowList())
            return;
    }

    int idx = calculateInitialIndex();
    if (idx >= 0)
        m_view.setCurrentIndex(idx);

    m_overlayState = OverlayState::Visible;
    emit stateChanged(m_overlayState);
    emit showRequested();
    m_stayOpenMode = (m_sessionInfo.endTrigger == SessionEndTrigger::ExplicitAction);
    qDebug() << "[OverlayCtrl] stayOpenMode=" << m_stayOpenMode
             << "endTrigger=" << (int)m_sessionInfo.endTrigger;
    Util::closeSystemWindows();

    QTimer::singleShot(0, this, [this]() {
        if (m_overlayState != OverlayState::Visible)
            return;
        forceShow();
    });
}

void OverlayController::hideWindow() {
    qInfo() << "[OverlayCtrl] hideWindow state=" << (int)m_overlayState;
    m_overlayState = OverlayState::Hidden;
    emit stateChanged(m_overlayState);
    emit hideRequested();
    m_listDirty = true;
    m_view.doHide();
}

bool OverlayController::refreshWindowList() {
    qDebug() << "[OverlayCtrl] refreshWindowList";
    bool ok = prepareListWidget();
    if (ok) {
        m_listDirty = false;
        qDebug() << "[OverlayCtrl] refreshWindowList ok, groupCount=" << m_model->groupCount();
    } else {
        qWarning() << "[OverlayCtrl] refreshWindowList failed";
    }
    return ok;
}

bool OverlayController::prepareListWidget() {
    auto winGroupList = m_windowManager->prepareWindowGroupList();
    m_view.updateGroups(winGroupList);

    if (m_model->groupCount() > 0) {
        bool displayOnPrimary = (cfg().getDisplayMonitor() == PrimaryMonitor);
        auto screen = displayOnPrimary ?
                      QGuiApplication::primaryScreen() :
                      QGuiApplication::screenAt(QCursor::pos());
        if (!screen && !displayOnPrimary) {
            qWarning() << "Cursor Screen nullptr! Fallback to primary";
            screen = QApplication::primaryScreen();
        }
        if (!screen) {
            sysTray().showMessage("Error", "Screen nullptr!");
            return false;
        }
        calculateGeometry(screen);
    } else {
        return false;
    }

    return true;
}

bool OverlayController::isVisible() const {
    return m_overlayState == OverlayState::Visible;
}

bool OverlayController::isForeground() const {
    return GetForegroundWindow() == m_view.hWnd();
}

void OverlayController::notifyForegroundChanged(HWND hwnd) {
    if (hwnd == m_view.hWnd()) return;
    if (!Util::isWindowAllowed(hwnd, true)) return;
    auto path = Util::getWindowProcessPath(hwnd);
    qInfo() << "Foreground window changed:"
            << Util::getWindowTitle(hwnd) << Util::getClassName(hwnd) << path
            << Util::getFileDescription(path);
    m_windowManager->recordWindowActivation(hwnd);
}

void OverlayController::warmupCache() {
    QElapsedTimer t;
    t.start();
    auto winGroupList = m_windowManager->prepareWindowGroupList();
    m_view.updateGroups(winGroupList);
    if (m_model->groupCount() > 0) {
        bool displayOnPrimary = (cfg().getDisplayMonitor() == PrimaryMonitor);
        auto screen = displayOnPrimary
                          ? QGuiApplication::primaryScreen()
                          : QGuiApplication::screenAt(QCursor::pos());
        if (!screen && !displayOnPrimary)
            screen = QApplication::primaryScreen();
        if (screen)
            calculateGeometry(screen);
    }
    m_listDirty = false;
    qInfo() << "[Startup] warmupCache" << t.elapsed() << "ms";
}
