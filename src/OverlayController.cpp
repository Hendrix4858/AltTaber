#include "OverlayController.h"
#include "WindowGroupModel.h"
#include "WindowManager.h"
#include "utils/Util.h"
#include "utils/ConfigManager.h"
#include "utils/SystemTray.h"
#include "utils/ThemeManager.h"
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
    : QObject(parent), m_widget(widget), m_lv(listView), m_label(labelWidget),
      m_model(model), m_wm(wm) {}

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

    auto* lv = qobject_cast<QListView*>(m_lv);
    if (!lv) return;

    auto screenGeo = screen->geometry();
    int maxWidth = screenGeo.width() - m_margin.left() - m_margin.right();
    int iconSize = lv->iconSize().width();
    int gridWidth = lv->gridSize().width();
    int totalNeeded = gridWidth * m_model->groupCount();
    int minIcon = cfg().getMinIconSize();

    if (totalNeeded > maxWidth) {
        int newGridWidth = maxWidth / m_model->groupCount();
        int padding = gridWidth - iconSize;
        int newIconSize = qMax(minIcon, newGridWidth - padding);
        int finalGridWidth = newIconSize + padding;

        lv->setIconSize({newIconSize, newIconSize});
        lv->setGridSize({finalGridWidth, lv->gridSize().height()});
    } else {
        lv->setIconSize({64, 64});
        lv->setGridSize({80, 80});
    }

    auto firstIndex = m_model->index(0);
    auto firstRect = lv->visualRect(firstIndex);
    auto width = lv->gridSize().width() * m_model->groupCount() + (firstRect.x() - lv->frameWidth());
    lv->setFixedWidth(width);

    auto lvRect = lv->rect();
    auto thisRect = lvRect.marginsAdded(m_margin);
    thisRect.moveCenter(screenGeo.center());

    m_widget->setGeometry(thisRect);

    lvRect.moveCenter(m_widget->rect().center());
    lv->move(lvRect.topLeft());
}

bool OverlayController::prepareListWidget() {
    auto* lv = qobject_cast<QListView*>(m_lv);
    if (!lv) return false;

    auto winGroupList = m_wm->prepareWindowGroupList();
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

    if (m_selectBackward && count >= 2) {
        lv->setCurrentIndex(m_model->index(count - 1));
        m_selectBackward = false;
    } else if (count >= 2) {
        auto foreWin = GetForegroundWindow();
        bool isFirstItemForeground = false;
        for (auto& info : winGroupList.at(0).windows) {
            if (info.hwnd == foreWin) {
                isFirstItemForeground = true;
                break;
            }
        }
        lv->setCurrentIndex(m_model->index(isFirstItemForeground ? 1 : 0));
    } else if (count == 1) {
        lv->setCurrentIndex(m_model->index(0));
    }

    return true;
}

void OverlayController::handleIntent(OverlayIntent intent) {
    reconcileState();
    qInfo() << "[OverlayCtrl] handleIntent intent=" << (int)intent
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
    qInfo() << "[Transition] state=" << (int)m_overlayState
            << "intent=" << (int)intent
            << "stayOpen=" << m_stayOpenMode
            << "listDirty=" << m_listDirty;

    switch (m_overlayState) {

    case OverlayState::Hidden:
        if (intent == OverlayIntent::ShowSwitcher || intent == OverlayIntent::FallbackShow ||
            intent == OverlayIntent::ShowSwitcherBackward) {
            m_selectBackward = (intent == OverlayIntent::ShowSwitcherBackward);
            qInfo() << "[Transition] Hidden + Show = refreshing list + show (backward="
                    << m_selectBackward << ")";
            m_listDirty = true;
            refreshWindowList();
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
    qInfo() << "[OverlayCtrl] showWindow state=" << (int)m_overlayState
            << "listDirty=" << m_listDirty;

    if (m_listDirty) {
        qInfo() << "[OverlayCtrl] list dirty, refreshing...";
        if (!refreshWindowList())
            return;
    }

    m_overlayState = OverlayState::Visible;
    m_stayOpenMode = (m_sessionInfo.endTrigger == SessionEndTrigger::ExplicitAction);
    qInfo() << "[OverlayCtrl] stayOpenMode=" << m_stayOpenMode
            << "endTrigger=" << (int)m_sessionInfo.endTrigger;
    Util::closeSystemWindows();
    qInfo() << "[OverlayCtrl] State -> Visible, calling forceShow...";
    bool ok = forceShow();
    qInfo() << "[OverlayCtrl] showWindow forceShow=" << ok;
    if (ok) {
        auto* lv = qobject_cast<QListView*>(m_lv);
        if (lv) {
            lv->setFocusPolicy(Qt::StrongFocus);
            lv->setFocus(Qt::OtherFocusReason);
            qInfo() << "[OverlayCtrl] QListView focus set";
        }
    } else {
        m_overlayState = OverlayState::Hidden;
        m_listDirty = true;
    }
}

void OverlayController::hideWindow() {
    qInfo() << "[OverlayCtrl] hideWindow state=" << (int)m_overlayState;
    m_overlayState = OverlayState::Hidden;
    m_listDirty = true;
    m_widget->hide();
}

bool OverlayController::refreshWindowList() {
    qInfo() << "[OverlayCtrl] refreshWindowList";
    bool ok = prepareListWidget();
    if (ok) {
        m_listDirty = false;
        qInfo() << "[OverlayCtrl] refreshWindowList ok, groupCount=" << m_model->groupCount();
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
    if (!Util::isWindowAcceptable(hwnd, true)) return;
    auto path = Util::getWindowProcessPath(hwnd);
    qInfo() << "Foreground window changed:"
            << Util::getWindowTitle(hwnd) << Util::getClassName(hwnd) << path
            << Util::getFileDescription(path);
    m_wm->recordWindowActivation(hwnd);
}

void OverlayController::warmupCache() {
    auto winGroupList = m_wm->prepareWindowGroupList();
    m_model->setGroups(winGroupList);
}
