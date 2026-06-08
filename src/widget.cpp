#include "widget.h"
#include "WindowGroupModel.h"
#include "WindowManager.h"
#include "SelectionController.h"
#include "TaskbarWindowCycler.h"
#include "GroupWindowCycler.h"
#include "ui_Widget.h"
#include "utils/Util.h"
#include "utils/HotkeyAction.h"
#include "utils/ConfigManager.h"
#include "utils/ThemeManager.h"
#include "utils/SystemTray.h"
#include "utils/setWindowBlur.h"
#include "utils/IconOnlyDelegate.h"
#include "utils/QtWin.h"
#include <QDebug>
#include <QWindow>
#include <QScreen>
#include <QPainter>
#include <QWheelEvent>
#include <QTimer>
#include <QThread>
#include <QElapsedTimer>

Widget::Widget(WindowManager* wm, QWidget* parent)
    : QWidget(parent), ui(new Ui::Widget), m_wm(wm) {
    QElapsedTimer t;
    t.start();
    ui->setupUi(this);
    lv = ui->listWidget;
    m_model = new WindowGroupModel(this);
    lv->setModel(m_model);
    setWindowFlag(Qt::WindowStaysOnTopHint);
    setWindowFlag(Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    QtWin::taskbarDeleteTab(this);
    setWindowTitle("AltTaber");

    Util::setWindowRoundCorner(this->hWnd());
    setWindowBlur(hWnd());

    setupLabelFont();

    lv->setViewMode(QListView::IconMode);
    lv->setMovement(QListView::Static);
    lv->setFlow(QListView::LeftToRight);
    lv->setWrapping(false);
    lv->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    lv->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    lv->setIconSize({64, 64});
    lv->setGridSize({80, 80});
    lv->setFixedHeight(lv->gridSize().height());
    lv->setUniformItemSizes(true);
    lv->setStyleSheet(R"(
        QListView {
            background-color: transparent;
            border: none;
            outline: none;
        }
    )");
    lv->setItemDelegate(new IconOnlyDelegate(lv));
    lv->installEventFilter(this);

    // Create controllers
    m_cyc = new GroupWindowCycler(this);
    m_overlayCtrl = new OverlayController(this, lv, ui->label, m_model, m_wm, this);
    m_selectCtrl = new SelectionController(lv, m_model, m_wm, m_cyc, this);
    m_selectCtrl->setLabelWidget(ui->label);
    m_taskbarCycler = new TaskbarWindowCycler(m_cyc, m_wm, this);

    connect(lv, &QListView::clicked, this, &Widget::handleListItemClicked);

    connect(qApp, &QApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        if (state == Qt::ApplicationInactive && isVisible()) {
            hideOverlay();
        }
    });

    connect(lv->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex& current, const QModelIndex&) {
        if (current.isValid()) m_selectCtrl->showLabelForItem(current);
    });

    connect(m_selectCtrl, &SelectionController::labelTextChanged, this, [this](const QString& text) {
        ui->label->setText(text);
        ui->label->setStyleSheet(QString("color: %1; background: transparent;")
                                  .arg(ThemeManager::current().textColor.name()));
        ui->label->adjustSize();
        auto idx = lv->currentIndex();
        if (idx.isValid()) {
            auto itemRect = lv->visualRect(idx);
            auto center = itemRect.center() + QPoint(0, itemRect.height() / 2 + ListWidgetMargin.bottom() / 2);
            center = lv->mapTo(this, center);
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
            m_selectCtrl->exitGroupWindowMode(true);
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
        m_wm->reloadFilterRules();
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
             << "currentRow=" << lv->currentIndex().row();

    // Check overlay bindings
    const auto& overlayBinds = m_overlayCtrl->overlayBindings();
    if (vk != 0) {
        for (auto it = overlayBinds.begin(); it != overlayBinds.end(); ++it) {
            for (const auto& b : it.value()) {
                if (b.vkCode == vk && b.modifiers == modifiers) {
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
    qDebug() << "[Release] keyReleaseEvent key=" << event->key()
             << "visible=" << isVisible()
             << "groupMode=" << m_selectCtrl->isInGroupMode()
             << "stayOpen=" << m_overlayCtrl->stayOpenMode()
             << "currentRow=" << lv->currentIndex().row();

    if (event->key() == Qt::Key_Alt) {
        if (m_lastAltReleaseTime.isValid() && m_lastAltReleaseTime.elapsed() < kAltReleaseDedupMs) {
            qDebug() << "[Release] Skipping duplicate Alt release";
            return;
        }
        m_lastAltReleaseTime.start();

        qInfo() << "[Release] Alt released"
                << "visible=" << isVisible()
                << "stayOpenOnAltRelease=" << cfg().getStayOpenOnAltRelease()
                << "isForeground=" << isForeground()
                << "groupMode=" << m_selectCtrl->isInGroupMode()
                << "stayOpenMode=" << m_overlayCtrl->stayOpenMode()
                << "currentRow=" << lv->currentIndex().row();

        m_cyc->clearGroupWindowOrder();
        m_selectCtrl->resetState();

        if (!isVisible()) {
            QWidget::keyReleaseEvent(event);
            return;
        }

        if (cfg().getStayOpenOnAltRelease()) {
            if (isForeground()) {
                qInfo() << "[Release] stayOpenOnAltRelease — keeping overlay open, no rebuild";
                m_overlayCtrl->setStayOpenMode(true);
                return;
            }
        }

        // Activate selected window, then hide
        if (m_selectCtrl->isInGroupMode()) {
            HWND targetHwnd = nullptr;
            QString targetExe, targetTitle;
            if (auto idx = lv->currentIndex(); idx.isValid())
                if (auto& g = m_model->groupAt(idx.row()); !g.windows.empty()) {
                    targetHwnd = g.windows.first().hwnd;
                    targetExe = g.exePath;
                    targetTitle = g.windows.first().title;
                }
            m_selectCtrl->exitGroupWindowMode(false);
            if (targetHwnd)
                activateWindowWithVerification(targetHwnd, targetExe, targetTitle);
        } else {
            activateCurrentGroupWindow();
        }
        m_overlayCtrl->handleIntent(OverlayIntent::AltReleased);
    }
    QWidget::keyReleaseEvent(event);
}

void Widget::hideEvent(QHideEvent* event) {
    m_overlayCtrl->setStayOpenMode(false);
    QWidget::hideEvent(event);
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

bool Widget::requestShow(OverlayIntent why) {
    qInfo() << "[Widget::requestShow] why=" << (int)why << "thread=" << QThread::currentThread();
    m_selectCtrl->resetAll();
    m_overlayCtrl->handleIntent(why);
    return m_overlayCtrl->overlayState() == OverlayController::OverlayState::Visible;
}

void Widget::hideOverlay() {
    qInfo() << "[Widget::hideOverlay]";
    m_overlayCtrl->handleIntent(OverlayIntent::Dismiss);
}

Widget::OverlayState Widget::overlayState() const {
    return m_overlayCtrl->overlayState();
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
    m_selectCtrl->handleOverlayAction(action, modifiers);
}

void Widget::handleGlobalAction(HotkeyAction action, Qt::KeyboardModifiers modifiers) {
    qInfo() << "[Widget::handleGlobalAction] ENTER action=" << hotkeyActionName(action)
            << "modifiers=" << modifiers
            << "overlayState=" << (int)m_overlayCtrl->overlayState()
            << "visible=" << isVisible()
            << "thread=" << QThread::currentThread();
    bool visible = isVisible();

    switch (action) {
    case HotkeyAction::ShowSwitcher:
        if (!visible) {
            qInfo() << "[Action] ShowSwitcher -> requestShow";
            requestShow();
        } else {
            qInfo() << "[Action] ShowSwitcher -> overlay visible -> CycleForward";
            m_selectCtrl->handleOverlayAction(HotkeyAction::CycleForward, modifiers);
        }
        return;
    case HotkeyAction::EnterGroupMode:
        if (visible && !isMinimized()) {
            qInfo() << "[Action] EnterGroupMode -> overlay action";
            m_selectCtrl->handleOverlayAction(HotkeyAction::EnterGroupMode, modifiers);
        } else {
            qInfo() << "[Action] EnterGroupMode -> cycle foreground windows";
            auto foreWin = GetForegroundWindow();
            auto& order = m_cyc->groupWindowOrder();
            if (order.isEmpty()) {
                auto targetExe = Util::getWindowProcessPath(foreWin);
                order = m_wm->filteredHwndsForExe(targetExe);
            }
            bool forward = !(modifiers & Qt::ShiftModifier);
            if (auto nextWin = GroupWindowCycler::rotateWindowInGroup(order, foreWin, forward)) {
                Util::switchToWindow(nextWin, true);
            }
        }
        return;
    case HotkeyAction::TogglePause:
        qInfo() << "[Action] TogglePause";
        cfg().setPaused(!cfg().getPaused());
        return;
    default:
        return;
    }
}

void Widget::updateOverlayBindings(const HotkeyBindings& bindings) {
    m_overlayCtrl->setOverlayBindings(bindings);
}

void Widget::setupLabelFont() {
    static auto reloadLabelFontCfg = [this] {
        const QStringList Fonts = {"Microsoft YaHei UI", "Microsoft YaHei", "Consolas"};
        auto labelFont = ui->label->font();
        labelFont.setPointSize(cfg().get("label/font_size", 10).toInt());
        auto defaultFF = QStringList{cfg().get("label/font_family", Fonts[0]).toString()};
        labelFont.setFamilies(defaultFF << Fonts.mid(1));
        ui->label->setFont(labelFont);
        qDebug() << labelFont.families();
        qDebug() << "Label Actual Font:" << QFontInfo(labelFont).family();
    };
    reloadLabelFontCfg();

    connect(&cfg(), &ConfigManager::configEdited, this, [] {
        reloadLabelFontCfg();
    });
}

void Widget::activateCurrentGroupWindow() {
    if (auto index = lv->currentIndex(); index.isValid())
        if (auto& group = m_model->groupAt(index.row()); !group.windows.empty())
            activateWindowWithVerification(group.windows.first().hwnd, group.exePath, group.windows.first().title);
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
