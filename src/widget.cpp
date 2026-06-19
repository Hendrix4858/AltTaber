#include "widget.h"
#include "WindowGroupModel.h"
#include "WindowManager.h"
#include "SelectionController.h"
#include "TaskbarWindowCycler.h"
#include "GroupWindowCycler.h"
#include "ui_Widget.h"
#include "utils/Util.h"
#include "core/HotkeyAction.h"
#include "core/ConfigManager.h"
#include "core/ThemeManager.h"
#include "lifecycle/SystemTray.h"
#include "utils/setWindowBlur.h"
#include "lifecycle/IconOnlyDelegate.h"
#include "lifecycle/QtWin.h"
#include "core/QuitReason.h"
#include <QApplication>
#include <QCloseEvent>
#include <QDebug>
#include <QWindow>
#include <QScreen>
#include <QPainter>
#include <QWheelEvent>
#include <QTimer>
#include <QThread>
#include <QElapsedTimer>

Widget::Widget(WindowManager* wm, QWidget* parent)
    : QWidget(parent), ui(new Ui::Widget), m_windowManager(wm) {
    QElapsedTimer t;
    t.start();
    ui->setupUi(this);
    m_listView = ui->listWidget;
    m_model = new WindowGroupModel(this);
    m_listView->setModel(m_model);
    setWindowFlag(Qt::WindowStaysOnTopHint);
    setWindowFlag(Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    QtWin::taskbarDeleteTab(this);
    setWindowTitle("AltTaber");

    Util::setWindowRoundCorner(this->hWnd());
    setWindowBlur(hWnd());

    setupLabelFont();

    m_listView->setViewMode(QListView::IconMode);
    m_listView->setMovement(QListView::Static);
    m_listView->setFlow(QListView::LeftToRight);
    m_listView->setWrapping(false);
    m_listView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_listView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_listView->setIconSize({64, 64});
    m_listView->setGridSize({80, 80});
    m_listView->setFixedHeight(m_listView->gridSize().height());
    m_listView->setUniformItemSizes(true);
    m_listView->setStyleSheet(R"(
        QListView {
            background-color: transparent;
            border: none;
            outline: none;
        }
    )");
    m_listView->setItemDelegate(new IconOnlyDelegate(m_listView));
    m_listView->installEventFilter(this);

    // Create controllers
    m_groupCycler = new GroupWindowCycler(this);
    m_overlayCtrl = new OverlayController(this, m_listView, ui->label, m_model, m_windowManager, this);
    m_selectCtrl = new SelectionController(m_listView, m_model, m_windowManager, m_groupCycler, this);
    m_selectCtrl->setLabelWidget(ui->label);
    m_taskbarCycler = new TaskbarWindowCycler(m_groupCycler, m_windowManager, this);

    // Relay OverlayController state signals outward for backward compatibility
    connect(m_overlayCtrl, &OverlayController::showRequested, this, &Widget::overlayShown);
    connect(m_overlayCtrl, &OverlayController::hideRequested, this, &Widget::overlayDismissed);

    connect(m_listView, &QListView::clicked, this, &Widget::handleListItemClicked);

    connect(qApp, &QApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        if (state == Qt::ApplicationInactive
            && m_overlayCtrl->overlayState() == OverlayController::OverlayState::Visible) {
            hideOverlay();
        }
    });

    connect(m_listView->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex& current, const QModelIndex&) {
        if (current.isValid()) m_selectCtrl->showLabelForItem(current);
    });

    connect(m_selectCtrl, &SelectionController::labelTextChanged, this, [this](const QString& text) {
        ui->label->setText(text);
        ui->label->setStyleSheet(QString("color: %1; background: transparent;")
                                  .arg(ThemeManager::current().textColor.name()));
        ui->label->adjustSize();
        auto idx = m_listView->currentIndex();
        if (idx.isValid()) {
            auto itemRect = m_listView->visualRect(idx);
            auto center = itemRect.center() + QPoint(0, itemRect.height() / 2 + m_listWidgetMargin.bottom() / 2);
            center = m_listView->mapTo(this, center);
            auto labelRect = ui->label->rect();
            labelRect.moveCenter(center);
            auto bound = this->rect().marginsRemoved({5, 0, 5, 0});
            labelRect.moveRight(qMin(labelRect.right(), bound.right()));
            labelRect.moveLeft(qMax(labelRect.left(), bound.left()));
            ui->label->move(labelRect.topLeft());
        }
    });

    connect(m_selectCtrl, &SelectionController::geometryNeedsRecalc, this, [this]() {
        m_overlayCtrl->recalculateGeometry();
    });

    connect(m_selectCtrl, &SelectionController::activateAndHide, this, [this]() {
        m_overlayCtrl->setStayOpenMode(false);
        if (m_selectCtrl->isInGroupMode()) {
            m_selectCtrl->collapseGroup(true);
            hideOverlay();
        } else {
            activateCurrentGroupWindow();
            hideOverlay();
        }
    });

    connect(m_selectCtrl, &SelectionController::dismiss, this, [this]() {
        hideOverlay();
    });

    connect(m_selectCtrl, &SelectionController::switchToWindowRequested, this,
            [this](HWND hwnd, const QString& exePath, const QString& title) {
        activateWindowWithVerification(hwnd, exePath, title);
    });

    connect(m_selectCtrl, &SelectionController::foregroundChanged, this,
            [this](HWND hwnd) {
        notifyForegroundChanged(hwnd);
    });

    connect(&cfg(), &ConfigManager::configEdited, this, [this]() {
        m_windowManager->reloadFilterRules();
    });

    connect(m_overlayCtrl, &OverlayController::sessionFinished, this, [this]() {
        qInfo() << "[sessionFinished] activating selected window";
        if (m_selectCtrl->isInGroupMode()) {
            HWND targetHwnd = nullptr;
            QString targetExePath, targetTitle;
            if (auto idx = m_listView->currentIndex(); idx.isValid())
                if (auto& g = m_model->groupAt(idx.row()); !g.windows.empty()) {
                    targetHwnd = g.windows.first().hwnd;
                    targetExePath = g.exePath;
                    targetTitle = g.windows.first().title;
                }
            m_selectCtrl->collapseGroup(false);
            if (targetHwnd)
                activateWindowWithVerification(targetHwnd, targetExePath, targetTitle);
        } else {
            activateCurrentGroupWindow();
        }
    });

    qInfo() << "Widget initialized in" << t.elapsed() << "ms";
}

Widget::~Widget() {
    delete ui;
}

void Widget::keyPressEvent(QKeyEvent* event) {
    auto key = event->key();
    auto modifiers = event->modifiers();
    quint32 vk = qtKeyToVkCode(key);

    qDebug() << "[Switch] keyPressEvent key=" << key << "modifiers=" << modifiers
             << "stayOpen=" << m_overlayCtrl->stayOpenMode()
             << "groupMode=" << m_selectCtrl->isInGroupMode()
             << "currentRow=" << m_listView->currentIndex().row();

    // Check overlay bindings
    const auto& overlayBinds = m_overlayCtrl->overlayBindings();
    if (vk != 0) {
        for (auto it = overlayBinds.begin(); it != overlayBinds.end(); ++it) {
            for (const auto& b : it.value()) {
                if (b.matches(vk, modifiers)) {
                    qDebug() << "[Widget] Overlay binding match:"
                             << hotkeyActionName(it.key())
                             << "vk=" << vk << "mods=" << modifiers;
                    handleOverlayAction(it.key(), modifiers);
                    return;
                }
            }
        }
    }

    // Letter jump
    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        if (m_selectCtrl->jumpToLetter(QChar(key)))
            return;
    }

    QWidget::keyPressEvent(event);
}

void Widget::keyReleaseEvent(QKeyEvent* event) {
    QWidget::keyReleaseEvent(event);
}

void Widget::hideEvent(QHideEvent* event) {
    m_overlayCtrl->setStayOpenMode(false);
    QWidget::hideEvent(event);
}

void Widget::closeEvent(QCloseEvent* event) {
    QuitReason::markIntentional();
    QApplication::quit();
    event->accept();
}

void Widget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(ThemeManager::current().widgetBg);
    painter.drawRect(rect());
}

void Widget::notifyForegroundChanged(HWND hwnd) {
    m_overlayCtrl->notifyForegroundChanged(hwnd);
}

void Widget::warmupCache() {
    m_overlayCtrl->warmupCache();
}

bool Widget::requestShow(OverlayIntent why, HotkeyAction triggeringAction) {
    qInfo() << "[Widget::requestShow] why=" << (int)why
            << "action=" << hotkeyActionName(triggeringAction)
            << "thread=" << QThread::currentThread();
    m_selectCtrl->resetAll();

    auto meta = getActionMetadata(triggeringAction);
    qInfo() << "[requestShow] action=" << hotkeyActionName(triggeringAction)
            << "lifecycle=" << (int)meta.lifecycle
            << "endTrigger=" << (int)meta.endTrigger;
    if (meta.lifecycle == HotkeyLifecycle::OverlaySession) {
        m_overlayCtrl->setSessionInfo({triggeringAction, meta.endTrigger});
        qInfo() << "[requestShow] sessionInfo set: endTrigger=" << (int)meta.endTrigger;
    }

    m_overlayCtrl->handleIntent(why);
    return m_overlayCtrl->overlayState() == OverlayController::OverlayState::Visible;
}

void Widget::hideOverlay() {
    qInfo() << "[Widget::hideOverlay]";
    m_overlayCtrl->handleIntent(OverlayIntent::Dismiss);
}

bool Widget::eventFilter(QObject* watched, QEvent* event) {
    if (m_selectCtrl->handleEventFilter(watched, event, m_overlayCtrl->stayOpenMode()))
        return true;

    return false;
}

void Widget::handleListItemClicked(const QModelIndex& index) {
    m_selectCtrl->handleListItemClicked(index, m_overlayCtrl->stayOpenMode());
}

void Widget::handleOverlayAction(HotkeyAction action, Qt::KeyboardModifiers modifiers) {
    m_selectCtrl->handleOverlayAction(action, modifiers, InputSource::KeyboardInput);
}

void Widget::handleHookOverlayAction(HotkeyAction action, Qt::KeyboardModifiers modifiers) {
    m_selectCtrl->handleOverlayAction(action, modifiers, InputSource::LowLevelHook);
}

void Widget::handleGlobalAction(HotkeyAction action, Qt::KeyboardModifiers modifiers) {
    bool visible = m_overlayCtrl->overlayState() == OverlayController::OverlayState::Visible;
    qInfo() << "[Widget::handleGlobalAction] ENTER action=" << hotkeyActionName(action)
            << "modifiers=" << modifiers
            << "visible=" << visible
            << "thread=" << QThread::currentThread();

    switch (action) {
    case HotkeyAction::SwitchToNextWindow:
        if (!visible) {
            qInfo() << "[Action] SwitchToNextWindow -> requestShow";
            requestShow(OverlayIntent::ShowSwitcher, action);
        } else {
            qInfo() << "[Action] SwitchToNextWindow -> overlay visible -> CycleForward";
            m_selectCtrl->handleOverlayAction(HotkeyAction::CycleForward, modifiers);
        }
        return;
    case HotkeyAction::CycleProcessWindows:
        if (!visible || isMinimized()) {
            auto foregroundHwnd = GetForegroundWindow();
            qInfo() << "[Action] CycleProcessWindows foregroundHwnd=" << Qt::hex << foregroundHwnd;
            auto targetExePath = Util::getWindowProcessPath(foregroundHwnd);
            auto hwnds = m_windowManager->filteredHwndsForExe(targetExePath);
            qInfo() << "[Action] CycleProcessWindows hwnds=" << hwnds.size();
            if (hwnds.size() >= 2) {
                requestShow(OverlayIntent::ShowSwitcher, action);
                m_selectCtrl->tryEnterGroupForWindow(foregroundHwnd);
            }
        }
        return;
    case HotkeyAction::SwitchProcessWindow:
        if (!visible || isMinimized()) {
            auto foregroundHwnd = GetForegroundWindow();
            qInfo() << "[Action] SwitchProcessWindow foregroundHwnd=" << Qt::hex << foregroundHwnd;
            auto& order = m_groupCycler->groupWindowOrder();
            if (order.isEmpty()) {
                auto targetExePath = Util::getWindowProcessPath(foregroundHwnd);
                order = m_windowManager->filteredHwndsForExe(targetExePath);
            }
            bool forward = !(modifiers & Qt::ShiftModifier);
            if (auto nextWin = GroupWindowCycler::rotateWindow(order, foregroundHwnd, forward)) {
                Util::switchToWindow(nextWin, true);
            }
        }
        return;
    case HotkeyAction::ExpandGroup:
        if (visible && !isMinimized()) {
            qInfo() << "[Action] ExpandGroup -> overlay action";
            m_selectCtrl->handleOverlayAction(HotkeyAction::ExpandGroup, modifiers);
        }
        return;
    case HotkeyAction::TogglePause:
        qInfo() << "[Action] TogglePause";
        cfg().setPaused(!cfg().getPaused());
        return;
    case HotkeyAction::ShowSwitcherStayOpen:
    {
        auto meta = getActionMetadata(action);
        qInfo() << "[Action] ShowSwitcherStayOpen"
                << "visible=" << visible
                << "lifecycle=" << (int)meta.lifecycle
                << "endTrigger=" << (int)meta.endTrigger;
        if (!visible) {
            requestShow(OverlayIntent::ShowSwitcher, action);
        } else {
            m_overlayCtrl->setStayOpenMode(true);
            m_selectCtrl->handleOverlayAction(HotkeyAction::CycleForward, modifiers);
            qInfo() << "[Action] ShowSwitcherStayOpen -> stayOpen + CycleForward";
        }
        return;
    }
    case HotkeyAction::SwitchToPreviousWindow:
        qInfo() << "[Action] SwitchToPreviousWindow"
                << "visible=" << visible;
        if (!visible) {
            requestShow(OverlayIntent::ShowSwitcherBackward, action);
        } else {
            m_selectCtrl->handleOverlayAction(HotkeyAction::CycleBackward, modifiers);
        }
        return;
    default:
        return;
    }
}

void Widget::updateOverlayBindings(const HotkeyBindings& bindings) {
    m_overlayCtrl->setOverlayBindings(bindings);
}

void Widget::setupLabelFont() {
    auto reloadLabelFontConfig = [this] {
        const QStringList Fonts = {"Microsoft YaHei UI", "Microsoft YaHei", "Consolas"};
        auto labelFont = ui->label->font();
        labelFont.setPointSize(cfg().get("label/font_size", 10).toInt());
        auto defaultFF = QStringList{cfg().get("label/font_family", Fonts[0]).toString()};
        labelFont.setFamilies(defaultFF << Fonts.mid(1));
        ui->label->setFont(labelFont);
    };
    reloadLabelFontConfig();

    connect(&cfg(), &ConfigManager::configEdited, this, [reloadLabelFontConfig] {
        reloadLabelFontConfig();
    });
}

void Widget::activateCurrentGroupWindow() {
    if (auto index = m_listView->currentIndex(); index.isValid())
        if (auto& group = m_model->groupAt(index.row()); !group.windows.empty())
            activateWindowWithVerification(group.windows.first().hwnd, group.exePath, group.windows.first().title);
}

void Widget::onActivationModifiersReleased() {
    bool visible = m_overlayCtrl->overlayState() == OverlayController::OverlayState::Visible;
    qInfo() << "[ActivationRelease] forwarding to controller"
            << "visible=" << visible
            << "groupMode=" << m_selectCtrl->isInGroupMode();

    m_groupCycler->clearGroupWindowOrder();
    m_selectCtrl->resetState();

    if (!visible) return;

    m_overlayCtrl->handleIntent(OverlayIntent::SessionEndConditionMet);
}

void Widget::activateWindowWithVerification(HWND targetHwnd, const QString& exePath,
                                             const QString& title) {
    qInfo().noquote() << "[SwitchTo] Selected:" << title
             << "hwnd=" << Qt::hex << targetHwnd << Qt::dec
             << "exe=" << exePath;
    Util::switchToWindow(targetHwnd, true);

    QTimer::singleShot(200, [targetHwnd, title, exePath]() {
        HWND actual = GetForegroundWindow();
        QString actualTitle = Util::getWindowTitle(actual);
        QString actualExe = Util::getWindowProcessPath(actual);
        bool match = (actual == targetHwnd);
        qInfo().noquote() << "[SwitchTo] Actual:  " << actualTitle
                 << "hwnd=" << Qt::hex << actual << Qt::dec
                 << "exe=" << actualExe
                 << (match ? "[✓]" : "[✗] MISMATCH");
    });
}
