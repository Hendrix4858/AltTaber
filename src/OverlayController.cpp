#include "OverlayController.h"
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

OverlayController::OverlayController(QWidget* widget, QWidget* listView, QWidget* labelWidget,
                                     WindowGroupModel* model, WindowManager* wm,
                                     QObject* parent)
    : QObject(parent), m_widget(widget), m_listView(listView), m_label(labelWidget),
      m_model(model), m_windowManager(wm) {}

void OverlayController::setOverlayBindings(const HotkeyBindings& bindings) {
    m_overlayBindings.clear();
    for (auto it = bindings.begin(); it != bindings.end(); ++it) {
        if (hotkeyActionScope(it.key()) == HotkeyScope::Overlay)
            m_overlayBindings[it.key()] = it.value();
    }
}

bool OverlayController::forceShow() {
    HWND hwnd = reinterpret_cast<HWND>(m_widget->winId());
    if (!m_widget->isVisible()) {
        m_widget->setWindowOpacity(0.005);
        m_widget->showMinimized();
        m_widget->showNormal();
        m_widget->setWindowOpacity(1);
    }
    SetForegroundWindow(hwnd);
    BringWindowToTop(hwnd);
    bool ok = m_widget->isVisible();
    qDebug().noquote() << "[forceShow]" << "visible=" << m_widget->isVisible()
             << "isForeground=" << (GetForegroundWindow() == hwnd)
             << "fgHwnd=" << Qt::hex << GetForegroundWindow() << Qt::dec
             << "selfHwnd=" << Qt::hex << hwnd << Qt::dec;
    return ok;
}

void OverlayController::recalculateGeometry(QScreen* screen) {
    if (!screen) {
        screen = m_widget->screen();
        if (!screen) screen = QGuiApplication::primaryScreen();
    }
    if (!screen) return;

    auto* listView = qobject_cast<QListView*>(m_listView);
    if (!listView) return;

    auto screenGeo = screen->geometry();
    int maxWidth = screenGeo.width() - m_overlayMargin.left() - m_overlayMargin.right();
    int iconSize = listView->iconSize().width();
    int gridWidth = listView->gridSize().width();
    int totalNeeded = gridWidth * m_model->groupCount();
    int minIcon = cfg().getMinIconSize();

    qDebug().noquote() << "[Layout] groups =" << m_model->groupCount()
             << "maxWidth =" << maxWidth
             << "totalNeeded =" << totalNeeded
             << "screenW =" << screenGeo.width();

    if (totalNeeded > maxWidth) {
        int newGridWidth = maxWidth / m_model->groupCount();
        constexpr int kIconPadding = 16;
        int newIconSize = qMax(minIcon, newGridWidth - kIconPadding);
        if (newIconSize > newGridWidth)
            newIconSize = newGridWidth;

        qDebug().noquote() << "[Layout] SHRINK: newGridWidth =" << newGridWidth
                 << "newIconSize =" << newIconSize;

        listView->setIconSize({newIconSize, newIconSize});
        listView->setGridSize({newGridWidth, listView->gridSize().height()});
    } else {
        qDebug().noquote() << "[Layout] FULL: iconSize = 64 gridWidth = 80";
        listView->setIconSize({64, 64});
        listView->setGridSize({80, 80});
    }

    qDebug().noquote()
        << "[LayoutDebug]"
        << "gridSize=" << listView->gridSize()
        << "iconSize=" << listView->iconSize()
        << "spacing=" << listView->spacing()
        << "viewport.margins=" << listView->viewport()->contentsMargins()
        << "width=" << listView->width()
        << "height=" << listView->height()
        << "groupCount=" << m_model->groupCount();

    auto firstIndex = m_model->index(0);
    auto firstRect = listView->visualRect(firstIndex);
    auto width = listView->gridSize().width() * m_model->groupCount() + (firstRect.x() - listView->frameWidth());
    listView->setFixedWidth(width);

    auto listViewRect = listView->rect();
    auto widgetGeometry = listViewRect.marginsAdded(m_overlayMargin);
    widgetGeometry.moveCenter(screenGeo.center());

    m_widget->setGeometry(widgetGeometry);

    listViewRect.moveCenter(m_widget->rect().center());
    listView->move(listViewRect.topLeft());
}

bool OverlayController::prepareListWidget() {
    auto* listView = qobject_cast<QListView*>(m_listView);
    if (!listView) return false;

    auto winGroupList = m_windowManager->prepareWindowGroupList();
    m_model->setGroups(winGroupList);

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
        recalculateGeometry(screen);
    } else {
        return false;
    }

    auto count = m_model->groupCount();

    if (m_wasInvokedBackward && count >= 2) {
        listView->setCurrentIndex(m_model->index(count - 1));
        m_wasInvokedBackward = false;
    } else if (count >= 2) {
        auto foregroundHwnd = GetForegroundWindow();
        bool isFirstItemForeground = false;
        for (auto& info : winGroupList.at(0).windows) {
            if (info.hwnd == foregroundHwnd) {
                isFirstItemForeground = true;
                break;
            }
        }
        listView->setCurrentIndex(m_model->index(isFirstItemForeground ? 1 : 0));
    } else if (count == 1) {
        listView->setCurrentIndex(m_model->index(0));
    }

    return true;
}

void OverlayController::handleIntent(OverlayIntent intent) {
    reconcileState();
    qDebug() << "[OverlayCtrl] handleIntent intent=" << (int)intent
             << "state=" << (int)m_overlayState;
    transition(intent);
}

// ─────────────────────────────────────────────────────────
//  Reconciliation — fix state/UI drift
// ─────────────────────────────────────────────────────────
void OverlayController::reconcileState() {
    bool uiVisible = m_widget->isVisible();
    bool stateVisible = (m_overlayState == OverlayState::Visible);

    if (stateVisible && !uiVisible) {
        qWarning() << "[Reconcile] state=Visible but widget hidden → resetting state to Hidden";
        m_overlayState = OverlayState::Hidden;
        m_listDirty = true;
    }

    if (!stateVisible && uiVisible) {
        qWarning() << "[Reconcile] state=Hidden but widget visible → hiding (should not happen)";
        m_widget->hide();
    }
}

// ─────────────────────────────────────────────────────────
//  State machine — the single decision point
//  Only this function may call showWindow/hideWindow/refreshWindowList
// ─────────────────────────────────────────────────────────
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
            qDebug() << "[Transition] Hidden +" << (int)intent << "→ no-op";
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

// ─────────────────────────────────────────────────────────
//  Action primitives — only called from transition()
// ─────────────────────────────────────────────────────────
void OverlayController::showWindow() {
    qInfo() << "[OverlayCtrl] showWindow state=" << (int)m_overlayState;

    if (m_listDirty) {
        qDebug() << "[OverlayCtrl] list dirty, refreshing...";
        if (!refreshWindowList())
            return;
    }

    m_overlayState = OverlayState::Visible;
    emit stateChanged(m_overlayState);
    m_stayOpenMode = (m_sessionInfo.endTrigger == SessionEndTrigger::ExplicitAction);
    qDebug() << "[OverlayCtrl] stayOpenMode=" << m_stayOpenMode
             << "endTrigger=" << (int)m_sessionInfo.endTrigger;
    Util::closeSystemWindows();
    qDebug() << "[OverlayCtrl] State -> Visible, calling forceShow...";
    bool ok = forceShow();
    qDebug() << "[OverlayCtrl] showWindow forceShow=" << ok;
    if (ok) {
        emit showRequested();
        auto* listView = qobject_cast<QListView*>(m_listView);
        if (listView) {
            listView->setFocusPolicy(Qt::StrongFocus);
            listView->setFocus(Qt::OtherFocusReason);
            qDebug() << "[OverlayCtrl] QListView focus set";
        }
    } else {
        m_overlayState = OverlayState::Hidden;
        emit stateChanged(OverlayState::Hidden);
        m_listDirty = true;
    }
}

void OverlayController::hideWindow() {
    qInfo() << "[OverlayCtrl] hideWindow state=" << (int)m_overlayState;
    m_overlayState = OverlayState::Hidden;
    emit stateChanged(m_overlayState);
    emit hideRequested();
    m_listDirty = true;
    m_widget->hide();
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

bool OverlayController::isVisible() const {
    return m_widget->isVisible();
}

bool OverlayController::isForeground() const {
    return GetForegroundWindow() == reinterpret_cast<HWND>(m_widget->winId());
}

void OverlayController::notifyForegroundChanged(HWND hwnd) {
    if (hwnd == reinterpret_cast<HWND>(m_widget->winId())) return;
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
    m_model->setGroups(winGroupList);
    m_listDirty = false;
    qInfo() << "[Startup] warmupCache" << t.elapsed() << "ms";
}
