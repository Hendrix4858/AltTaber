#include "widget.h"
#include "WindowGroupModel.h"
#include "WindowManager.h"
#include "ui_Widget.h"
#include "utils/Util.h"
#include "utils/HotkeyAction.h"
#include <QDebug>
#include <QWindow>
#include <QScreen>
#include "utils/setWindowBlur.h"
#include "utils/IconOnlyDelegate.h"
#include <QPainter>
#include "utils/QtWin.h"
#include <QWheelEvent>
#include <QTimer>
#include <QMetaEnum>
#include "utils/SystemTray.h"
#include "utils/ConfigManager.h"
#include "utils/ThemeManager.h"
#include <QElapsedTimer>

Widget::Widget(WindowManager* wm, QWidget* parent) : QWidget(parent), ui(new Ui::Widget), m_wm(wm) {
    QElapsedTimer t;
    t.start();
    ui->setupUi(this);
    lv = ui->listWidget;
    m_model = new WindowGroupModel(this);
    lv->setModel(m_model);
    setWindowFlag(Qt::WindowStaysOnTopHint);
    setWindowFlag(Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground); //设置窗口背景透明 !但是会造成show()时的闪烁 和 绘制延迟(?)
    QtWin::taskbarDeleteTab(this); //删除任务栏图标
    setWindowTitle("AltTaber");

    Util::setWindowRoundCorner(this->hWnd()); // 设置窗口圆角
    setWindowBlur(hWnd()); // 设置窗口模糊, 必须配合Qt::WA_TranslucentBackground

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
    lv->setUniformItemSizes(true); // optimization ?
    lv->setStyleSheet(R"(
        QListView {
            background-color: transparent;
            border: none;
            outline: none; /* 去除选中时的虚线框（在文字为空时，会形成闪电一样的标志 离谱） */
        }
    )");
    // 就算Text为Null，也会占用空间，很难做到真正的IConMode，所以只能delegate自绘
    // 本来为了去除图标选中变色样式，可以对Icon手动addPixmap(..., QIcon::Selected) or (& ~Qt::ItemIsSelectable)
    // 但是采用delegate后，就没必要了
    // will not take ownership of delegate
    lv->setItemDelegate(new IconOnlyDelegate(lv));
    connect(lv, &QListView::clicked, this, &Widget::handleListItemClicked);

    connect(qApp, &QApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        if (state == Qt::ApplicationInactive && isVisible()) {
            hide();
        }
    });

    connect(lv->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex& current, const QModelIndex&) {
        if (current.isValid()) showLabelForItem(current);
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
             << "stayOpen=" << m_stayOpenMode << "groupMode=" << m_isInGroupWindowMode
             << "currentRow=" << lv->currentIndex().row();

    // Check overlay bindings first
    if (vk != 0) {
        for (auto it = m_overlayBindings.begin(); it != m_overlayBindings.end(); ++it) {
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
        qDebug() << "[Widget] No overlay binding for vk=" << vk
                 << "qtKey=" << key << "mods=" << modifiers;
    }

    // Letter jump (A-Z only if no overlay binding consumed it)
    if (key >= Qt::Key_A && key <= Qt::Key_Z && cfg().getLetterJumpEnabled()) {
        QChar pressedLetter = QChar(key).toUpper();

        QList<int> matchingIndices;
        for (int i = 0; i < m_model->groupCount(); ++i) {
            const auto& group = m_model->groupAt(i);
            QString appName = Util::getFileDescription(group.exePath);
            if (!appName.isEmpty() && Util::getDisplayFirstLetter(appName) == pressedLetter)
                matchingIndices.append(i);
        }

        if (matchingIndices.size() >= 1) {
            int currentPos = matchingIndices.indexOf(m_jumpLastIndex);
            int newPos = (pressedLetter == m_jumpLastLetter && currentPos >= 0)
                             ? (currentPos + 1) % matchingIndices.size()
                             : 0;
            int targetIndex = matchingIndices[newPos];
            lv->setCurrentIndex(m_model->index(targetIndex));
            showLabelForItem(m_model->index(targetIndex));
            m_jumpLastLetter = pressedLetter;
            m_jumpLastIndex = targetIndex;
        }
        return;
    }

    QWidget::keyPressEvent(event);
}

void Widget::lazyLoadIcons() {
    m_iconLoadIndex = 0;
    m_iconsFullyLoaded = false;
    ++m_iconLoadGeneration;
    auto gen = m_iconLoadGeneration;
    QTimer::singleShot(0, this, [this, gen] { loadNextIcon(gen); });
}

void Widget::loadNextIcon(int generation) {
    if (generation != m_iconLoadGeneration || m_iconsFullyLoaded) return;

    int count = m_model->groupCount();
    if (m_iconLoadIndex >= count) {
        m_iconsFullyLoaded = true;
        return;
    }

    int row = m_iconLoadIndex++;
    auto group = m_model->groupAt(row);
    auto hwnd = group.windows.isEmpty() ? nullptr : group.windows.first().hwnd;
    QIcon icon = Util::getCachedIcon(group.exePath, hwnd);
    if (!icon.isNull()) {
        m_model->setGroupIcon(row, icon);
    }

    auto gen = m_iconLoadGeneration;
    QTimer::singleShot(0, this, [this, gen] { loadNextIcon(gen); });
}

bool Widget::forceShow() {
    if (!isVisible()) {
        setWindowOpacity(0.005);
        showMinimized();
        showNormal();
        setWindowOpacity(1);
    } else {
        SetForegroundWindow(hWnd());
    }
    return isForeground();
}

/// show App description under the icon
void Widget::showLabelForItem(const QModelIndex& index, QString text) {
    if (!index.isValid()) return;

    if (text.isNull()) {
        if (m_isInGroupWindowMode) {
            auto& group = m_model->groupAt(index.row());
            if (!group.windows.isEmpty())
                text = group.windows.first().title;
        }
        if (text.isNull()) {
            auto path = m_model->groupAt(index.row()).exePath;
            text = Util::getFileDescription(path);
        }
    }
    ui->label->setText(text);
    ui->label->setStyleSheet(QString("color: %1; background: transparent;")
                              .arg(ThemeManager::current().textColor.name()));
    ui->label->adjustSize();

    auto itemRect = lv->visualRect(index);
    auto center = itemRect.center() + QPoint(0, itemRect.height() / 2 + ListWidgetMargin.bottom() / 2);
    center = lv->mapTo(this, center);
    auto labelRect = ui->label->rect();
    labelRect.moveCenter(center);

    auto bound = this->rect().marginsRemoved({5, 0, 5, 0});
    labelRect.moveRight(qMin(labelRect.right(), bound.right()));
    labelRect.moveLeft(qMax(labelRect.left(), bound.left())); // left align

    ui->label->move(labelRect.topLeft());
}

void Widget::setupLabelFont() {
    static auto reloadLabelFontCfg = [this] {
        const QStringList Fonts = {"Microsoft YaHei UI", "Microsoft YaHei", "Consolas"}; // fallback
        auto labelFont = ui->label->font();
        labelFont.setPointSize(cfg().get("label/font_size", 10).toInt());
        auto defaultFF = QStringList{cfg().get("label/font_family", Fonts[0]).toString()};
        labelFont.setFamilies(defaultFF << Fonts.mid(1));
        ui->label->setFont(labelFont);
        qDebug() << labelFont.families();
        qDebug() << "Label Actual Font:" << QFontInfo(labelFont).family();
    };
    reloadLabelFontCfg();

    // auto reload
    connect(&cfg(), &ConfigManager::configEdited, this, [] {
        reloadLabelFontCfg();
    });
}

void Widget::recalculateGeometry(QScreen* screen) {
    if (!screen) {
        screen = this->screen();
        if (!screen) screen = QGuiApplication::primaryScreen();
    }
    if (!screen) return;

    auto screenGeo = screen->geometry();
    int maxWidth = screenGeo.width() - ListWidgetMargin.left() - ListWidgetMargin.right();
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
    auto thisRect = lvRect.marginsAdded(ListWidgetMargin);
    thisRect.moveCenter(screenGeo.center());

    this->setGeometry(thisRect);

    lvRect.moveCenter(this->rect().center());
    lv->move(lvRect.topLeft());
}

void Widget::enterGroupWindowMode() {
    auto index = lv->currentIndex();
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

    recalculateGeometry();
    lv->setCurrentIndex(m_model->index(0));
    showLabelForItem(m_model->index(0), group.windows.first().title);
    qInfo() << "(Alt+`)Window focus:" << group.exePath << filtered.size() << "windows";
}

void Widget::exitGroupWindowMode(bool activateSelected) {
    if (!m_isInGroupWindowMode) {
        qDebug() << "[GroupMode] exitGroupWindowMode called but NOT in group mode";
        return;
    }
    qInfo() << "[GroupMode] exitGroupWindowMode activateSelected=" << activateSelected
            << "backupIndex=" << m_backupGroupIndex
            << "backupCount=" << m_backupGroupList.size();
    m_isInGroupWindowMode = false;

    if (activateSelected && this->isVisible()) {
        if (auto index = lv->currentIndex(); index.isValid()) {
            if (auto group = m_model->groupAt(index.row()); !group.windows.empty()) {
                qInfo() << "[GroupMode] activating window on exit";
                Util::switchToWindow(group.windows.first().hwnd, true);
            }
        }
        hide();
    }

    m_model->setGroups(m_backupGroupList);
    m_backupGroupList.clear();

    recalculateGeometry();
    int restoreIndex = qMin(m_backupGroupIndex, m_model->groupCount() - 1);
    lv->setCurrentIndex(m_model->index(restoreIndex));
    showLabelForItem(m_model->index(restoreIndex));
    m_backupGroupIndex = 0;

    qInfo() << "[GroupMode] Exit, restored to index" << restoreIndex;
}

void Widget::keyReleaseEvent(QKeyEvent* event) {
    qDebug() << "[Release] keyReleaseEvent key=" << event->key()
             << "visible=" << isVisible()
             << "groupMode=" << m_isInGroupWindowMode
             << "stayOpen=" << m_stayOpenMode
             << "currentRow=" << lv->currentIndex().row();
    if (event->key() == Qt::Key_Alt) {
        m_jumpLastLetter = {};
        m_jumpLastIndex = -1;
        m_wm->clearGroupWindowOrder(); // for Alt + `

        bool wasActive = (m_overlayState == OverlayState::Visible);
        m_overlayState = OverlayState::Hidden;

        if (wasActive) {
            if (cfg().getStayOpenOnAltRelease()) {
                if (isForeground()) {
                    m_stayOpenMode = true;
                    return;
                }
                // 覆盖层不在前台时,保持打开无意义,直接隐藏
            }
            if (m_isInGroupWindowMode) {
                // Window focus mode: get selected window before restoring model
                HWND targetHwnd = nullptr;
                QString targetExe;
                if (auto index = lv->currentIndex(); index.isValid()) {
                    if (auto group = m_model->groupAt(index.row()); !group.windows.empty()) {
                        targetHwnd = group.windows.first().hwnd;
                        targetExe = group.exePath;
                    }
                }
                exitGroupWindowMode(false); // restore full list
                if (targetHwnd) {
                    Util::switchToWindow(targetHwnd);
                    qInfo() << "Switch to" << WindowInfo{{}, {}, targetHwnd} << targetExe;
                }
            } else {
                // active selected window
                if (auto index = lv->currentIndex(); index.isValid()) {
                    if (auto group = m_model->groupAt(index.row()); !group.windows.empty()) {
                        WindowInfo targetWin = group.windows.at(0);
                        if (targetWin.hwnd) {
                            Util::switchToWindow(targetWin.hwnd);
                            qInfo() << "Switch to" << targetWin << group.exePath;
                        }
                    }
                }
            }
            hide(); //! must hide after active target window, or focus may fallback to prev foreground window
        }
    }
    QWidget::keyReleaseEvent(event);
}

void Widget::hideEvent(QHideEvent* event) {
    m_overlayState = OverlayState::Hidden;
    m_stayOpenMode = false;
    QWidget::hideEvent(event);
}

void Widget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(ThemeManager::current().widgetBg);
    painter.drawRect(rect());
}

/// 通知前台窗口变化
void Widget::notifyForegroundChanged(HWND hwnd) {
    if (hwnd == this->hWnd()) return;
    if (!Util::isWindowAcceptable(hwnd, true)) return;
    auto path = Util::getWindowProcessPath(hwnd);
    qInfo() << "Foreground window changed:"
            << Util::getWindowTitle(hwnd) << Util::getClassName(hwnd) << path << Util::getFileDescription(path);
    m_wm->recordWindowActivation(hwnd);
}

bool Widget::prepareListWidget() {
    m_jumpLastLetter = {};
    m_jumpLastIndex = -1;
    m_isInGroupWindowMode = false;
    m_backupGroupList.clear();
    m_backupGroupIndex = 0;
    auto winGroupList = m_wm->prepareWindowGroupList();
    m_model->setGroups(winGroupList);
    lazyLoadIcons();

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
    if (count >= 2) {
        auto foreWin = GetForegroundWindow();
        bool isFirstItemForeground = false;
        for (auto& info: winGroupList.at(0).windows) {
            if (info.hwnd == foreWin) {
                isFirstItemForeground = true;
                break;
            }
        }
        lv->setCurrentIndex(m_model->index(isFirstItemForeground ? 1 : 0));
        qDebug() << "[Show] initIndex=" << (isFirstItemForeground ? 1 : 0)
                 << "firstIsForeground=" << isFirstItemForeground
                 << "count=" << count;
    } else if (count == 1) {
        lv->setCurrentIndex(m_model->index(0));
        qDebug() << "[Show] initIndex=0 count=1";
    } else {
        qDebug() << "[Show] no windows!";
    }

    return true;
}

void Widget::warmupCache() {
    auto winGroupList = m_wm->prepareWindowGroupList();
    m_model->setGroups(winGroupList);
    lazyLoadIcons();
}

bool Widget::requestShow() {
    switch (m_overlayState) {
    case OverlayState::Showing:
        return true;
    case OverlayState::Visible:
        Util::closeSystemWindows();
        return forceShow();
    case OverlayState::Hiding:
        return false;
    case OverlayState::Hidden:
        break;
    }

    m_overlayState = OverlayState::Showing;
    m_stayOpenMode = false;
    Util::closeSystemWindows();
    bool ok = prepareListWidget() && forceShow();
    m_overlayState = ok ? OverlayState::Visible : OverlayState::Hidden;
    qInfo() << "[Show] result=" << ok
             << "state=" << (ok ? "Visible" : "Hidden")
             << "initialRow=" << lv->currentIndex().row();
    return ok;
}

bool Widget::eventFilter(QObject* watched, QEvent* event) {
    if (watched == lv) {
        if (event->type() == QEvent::KeyPress) {
            auto* keyEvent = static_cast<QKeyEvent*>(event);
            auto key = keyEvent->key();
            qDebug() << "[Filter] KeyPress on lv: key=" << key
                     << "stayOpen=" << m_stayOpenMode;
        }
        if (m_stayOpenMode && event->type() == QEvent::KeyPress) {
            auto* keyEvent = static_cast<QKeyEvent*>(event);
            auto key = keyEvent->key();
            auto modifiers = keyEvent->modifiers();

            if (key == Qt::Key_Return || key == Qt::Key_Enter) {
                m_stayOpenMode = false;
                if (m_isInGroupWindowMode) {
                    exitGroupWindowMode(true);
                } else {
                    if (auto index = lv->currentIndex(); index.isValid()) {
                        if (auto group = m_model->groupAt(index.row()); !group.windows.empty()) {
                            Util::switchToWindow(group.windows.first().hwnd, true);
                        }
                    }
                    hide();
                }
                return true;
            }
            if (key == Qt::Key_Escape) {
                m_stayOpenMode = false;
                hide();
                return true;
            }
            if (key == Qt::Key_Tab) {
                auto i = lv->currentIndex().row();
                bool isShiftPressed = (modifiers & Qt::ShiftModifier);
                auto count = m_model->groupCount();
                if (count <= 0) return false;
                auto index = (i - (2 * isShiftPressed - 1) + count) % count;
                lv->setCurrentIndex(m_model->index(index));
                return true;
            }
        }
        if (event->type() == QEvent::Wheel) {
        auto* wheelEvent = static_cast<QWheelEvent*>(event);
        auto cursorPos = wheelEvent->position().toPoint();
        if (auto index = lv->indexAt(cursorPos); index.isValid()) {
            if (lv->currentIndex() != index)
                lv->setCurrentIndex(index);
            auto windowGroup = m_model->groupAt(index.row());
            if (windowGroup.windows.isEmpty()) return false;

            if (lastWheelRow != index.row()) {
                lastWheelRow = index.row();
                lastWheelHwnd = nullptr;
                m_wm->clearGroupWindowOrder();
            }
            auto targetExe = windowGroup.exePath;
            bool isRollUp = wheelEvent->angleDelta().x() > 0;
            auto& order = m_wm->groupWindowOrder();
            if (order.isEmpty())
                order = m_wm->buildGroupWindowOrder(targetExe);

            if (!lastWheelHwnd) { // first time
                lastWheelHwnd = order.first();
            } else {
                if (lastWheelIsRollUp == isRollUp)
                    lastWheelHwnd = WindowManager::rotateWindowInGroup(order, lastWheelHwnd, isRollUp);
            }
            lastWheelIsRollUp = isRollUp;

            HWND nextFocus = lastWheelHwnd; // this隐藏后的焦点备选窗口, for `swtichToWindow` after AltUp
            if (isRollUp) {
                Util::bringWindowToTop(lastWheelHwnd, this->hWnd()); // without activate
            } else {
                auto& orderForNormal = m_wm->groupWindowOrder();
                if (auto normal = WindowManager::rotateNormalWindowInGroup(orderForNormal, lastWheelHwnd, false)) {
                    ShowWindow(normal, SW_SHOWMINNOACTIVE); // minimize
                    lastWheelHwnd = normal;
                    nextFocus = lastWheelHwnd;
                }
                if (auto normal = WindowManager::rotateNormalWindowInGroup(orderForNormal, lastWheelHwnd, false))
                    nextFocus = normal; // 备选焦点切换为下一个非最小化窗口 after AltUp
            }
            notifyForegroundChanged(nextFocus);
            showLabelForItem(index, Util::getWindowTitle(nextFocus));
            qDebug() << "Wheel" << isRollUp << Util::getWindowTitle(nextFocus) << lastWheelHwnd;

                return true; // stop propagation
            }
        }
    }
    return false;
}

/// Must hide after active target window, or focus may fallback to prev foreground window
void Widget::activateCurrentAndHide() {
    if (m_isInGroupWindowMode) {
        exitGroupWindowMode(true);
    } else {
        if (auto index = lv->currentIndex(); index.isValid()) {
            if (auto group = m_model->groupAt(index.row()); !group.windows.empty()) {
                Util::switchToWindow(group.windows.first().hwnd, true);
            }
        }
        hide();
    }
}

/// `forward`: true for restore, false for minimize
void Widget::rotateTaskbarWindowInGroup(const QString& exePath, bool forward, int windows) {
    qDebug() << "(Taskbar)Wheel on:" << exePath << forward << windows;
    if (exePath.isEmpty()) return;
    if (!windows) { // 程序没有打开的窗口，处于关闭状态; 若不拦截，可能造成错误窗口被触发：explorer.exe -> msedge.exe
        qDebug() << "No window for this app";
        return;
    }

    if (lastTaskbarPath != exePath) {
        lastTaskbarPath = exePath;
        m_wm->clearGroupWindowOrder();
    }
    auto& tbOrder = m_wm->groupWindowOrder();
    if (tbOrder.isEmpty()) {
        tbOrder = m_wm->buildGroupWindowOrder(exePath);
        lastTaskbarHwnd = nullptr;
    }

    if (tbOrder.isEmpty()) {
        qWarning() << "No window in group" << exePath;
        // 有些软件的窗口是由子进程创建的，如 steam.exe -> steamwebhelper.exe (持有窗口)
        // 但是在任务栏只能获取到父进程steam.exe
        // 这种情况下，需要查找其子进程的路径
        auto childPaths = Util::getChildProcessPaths(exePath);
        if (childPaths.isEmpty()) return;
        if (childPaths.size() == 1) {
            qDebug() << "Try to switch to child process:" << childPaths.first();
                tbOrder = m_wm->buildGroupWindowOrder(childPaths.first());
        } else {
            // 如果有多个子进程路径，就根据validWindows过滤
            qWarning() << "Multiple child processes" << childPaths;
            QSet<QString> validPaths;
            // If range-initializer returns a temporary, its lifetime is extended until the end of the loop
            for (auto hwnd: Util::listValidWindows()) {
                if (auto path = Util::getWindowProcessPath(hwnd); !path.isEmpty())
                    validPaths.insert(path.toLower());
            }
            for (auto& path: childPaths) {
                if (validPaths.contains(path.toLower())) {
                    qDebug() << "Try to switch to valid child process:" << path;
                    tbOrder = m_wm->buildGroupWindowOrder(path);
                    break;
                }
            }
        }
        // TODO 有可能a进程开启b进程之后，a就关闭了，他俩也没有真的父子关系
        //  例如：ksolaunch.exe -> wps.exe
        //  此时只能通过File Description来匹配，均为“WPS Office”
        if (tbOrder.isEmpty()) // 无力回天
            return;
    }

    HWND hwnd = nullptr;
    if (!lastTaskbarHwnd) {
        hwnd = tbOrder.first();
        if (forward && hwnd == GetForegroundWindow())
            hwnd = WindowManager::rotateWindowInGroup(tbOrder, hwnd, true);
    } else {
        if (lastTaskbarForward == forward)
            hwnd = WindowManager::rotateWindowInGroup(tbOrder, lastTaskbarHwnd, forward);
        else
            hwnd = lastTaskbarHwnd;
    }
    lastTaskbarForward = forward;

    static auto mouseEvent = [](DWORD flag) {
        mouse_event(flag, 0, 0, 0, 0);
    };
    if (forward) {
        if (windows == 1) { // 由于过滤的存在，groupWindowOrder.size() 不一定等于 windows(真实窗口数量)
            // 单窗口情况下，模拟点击呼出，是最保险的
            if ((hwnd != GetForegroundWindow() || IsIconic(hwnd))) { // 若采用SW_SHOWMINNOACTIVE, 则前台窗口不会变化，可能为刚刚最小化的窗口
                mouseEvent(MOUSEEVENTF_LEFTDOWN);
                mouseEvent(MOUSEEVENTF_LEFTUP);
                // 鼠标事件在下一轮事件循环中自然处理，无需 processEvents 强制处理（避免重入风险）
                qDebug() << "(Taskbar)Switch by click";
            }
        } else {
            // 在TaskListThumbnailWnd显示的情况下restore window会导致预览实时刷新，导致卡顿和闪烁
            // 隐藏TaskListThumbnailWnd也无效，会自动show
            // DwmSetWindowAttribute[DWMWA_FORCE_ICONIC_REPRESENTATION, DWMWA_DISALLOW_PEEK], 效果都不好，还是会刷新闪烁

            // 只能采用偷鸡hack，按住左键的情况下，预览窗口会消失
            if (HWND thumbnail = Util::getCurrentTaskListThumbnailWnd(); IsWindowVisible(thumbnail)) {
                qDebug() << "(Taskbar)Press LButton";
                mouseEvent(MOUSEEVENTF_LEFTDOWN);
                // 由于本程序hook了mouse，所以必须处理全局鼠标事件（in事件循环）
                QTimer::singleShot(20, this, [hwnd]() {
                    Util::switchToWindow(hwnd, true); // TODO thumbnail隐藏之前 不要switch，并且block滚轮 防止闪烁卡顿
                });
            } else
                Util::switchToWindow(hwnd, true);

            if (!taskbarTimer) {
                taskbarTimer = new QTimer(this);
                taskbarTimer->setSingleShot(true);
                taskbarTimer->setInterval(200);
                // TODO cursor移动后立即释放 防止拖拽
                connect(taskbarTimer, &QTimer::timeout, this, [this]() {
                    mouseEvent(MOUSEEVENTF_LEFTUP);
                    qDebug() << "(Taskbar)Release LButton";

                    // 鼠标点击thumbnail之后，其获取焦点，此时若焦点在其窗口组成员中，thumbnail就不会隐藏，这是Windows机制
                    // 只能通过将焦点转移到Taskbar使其隐藏
                    // 直接 HIDE thumbnail 不太行，会导致之后restore窗口时 thumbnail刷新 + 窗口闪烁，闪瞎了
                    QTimer::singleShot(100, this, []() {
                        // 等待thumbnail显示
                        if (HWND thumbnail = Util::getCurrentTaskListThumbnailWnd(); IsWindowVisible(thumbnail)) {
                            if (HWND taskbar = FindWindow(L"Shell_TrayWnd", nullptr))
                                Util::switchToWindow(taskbar, true);
                        }
                    });
                });
            }
            taskbarTimer->stop();
            taskbarTimer->start();
        }
        qDebug() << "(Taskbar)Switch to" << hwnd << Util::getWindowTitle(hwnd) << Util::getClassName(hwnd);
    } else {
        if (auto normal = WindowManager::rotateNormalWindowInGroup(tbOrder, hwnd, false)) {
            if (normal != hwnd)
                qDebug() << "(Taskbar)Skip minimized" << hwnd << "->" << normal;
            hwnd = normal;
            ShowWindow(hwnd, SW_MINIMIZE); // SW_MINIMIZE 会让焦点自动回落到下一个窗口
            // 当所有窗口隐藏后，getElementUnderMouse() 会变成"CEF-OSC-WIDGET"，但是焦点和前台窗口并不是他，离谱
            // 此时Automation对鼠标下任务栏Element的判定会出错，solution为手动变焦到任务栏（见TaskbarWheelHooker.cpp）
            // SW_SHOWMINNOACTIVE不会切换焦点，即便本窗口已经最小化，但仍然持有焦点；但这不是合理的行为，同时会让QQ Follower反复弹出
            qDebug() << "(Taskbar)Minimize" << hwnd << Util::getWindowTitle(hwnd) << Util::getClassName(hwnd);
        }
    }

    lastTaskbarHwnd = hwnd;
}

void Widget::clearGroupWindowOrder() {
    m_wm->clearGroupWindowOrder();
}

void Widget::updateOverlayBindings(const HotkeyBindings& bindings) {
    // Store only overlay-scope actions
    m_overlayBindings.clear();
    static const HotkeyAction overlayActions[] = {
        HotkeyAction::CycleForward,
        HotkeyAction::CycleBackward,
        HotkeyAction::MoveSelectionUp,
        HotkeyAction::MoveSelectionDown,
        HotkeyAction::ActivateSelected,
        HotkeyAction::DismissSwitcher,
        HotkeyAction::EnterGroupMode,
    };
    for (auto action : overlayActions) {
        if (bindings.contains(action))
            m_overlayBindings[action] = bindings[action];
    }
}

void Widget::handleOverlayAction(HotkeyAction action, Qt::KeyboardModifiers modifiers) {
    qInfo() << "[Action] overlayAction=" << hotkeyActionName(action)
            << "visible=" << isVisible()
            << "modifiers=" << modifiers << "groupMode=" << m_isInGroupWindowMode;
    if (!isVisible()) return;

    switch (action) {
    case HotkeyAction::CycleForward: {
        if (m_isInGroupWindowMode && (modifiers & Qt::AltModifier)) {
            // Exit group mode and advance to next group
            int savedIndex = m_backupGroupIndex;
            bool shiftPressed = (modifiers & Qt::ShiftModifier);
            exitGroupWindowMode(false);
            int count = m_model->groupCount();
            if (count <= 0) return;
            int nextIndex = shiftPressed
                ? (savedIndex - 1 + count) % count
                : (savedIndex + 1) % count;
            lv->setCurrentIndex(m_model->index(nextIndex));
            showLabelForItem(m_model->index(nextIndex));
            return;
        }
        auto i = lv->currentIndex().row();
        bool shiftPressed = (modifiers & Qt::ShiftModifier);
        auto count = m_model->groupCount();
        if (count <= 0) return;
        auto nextIndex = (i - (2 * (int)shiftPressed - 1) + count) % count;
        lv->setCurrentIndex(m_model->index(nextIndex));
        qDebug() << "[Action] CycleForward:" << i << "->" << nextIndex;
        if (!(modifiers & Qt::AltModifier))
            showLabelForItem(m_model->index(nextIndex));
        return;
    }
    case HotkeyAction::CycleBackward: {
        auto i = lv->currentIndex().row();
        auto count = m_model->groupCount();
        if (count <= 0) return;
        auto nextIndex = (i - 1 + count) % count;
        lv->setCurrentIndex(m_model->index(nextIndex));
        showLabelForItem(m_model->index(nextIndex));
        qDebug() << "[Action] CycleBackward:" << i << "->" << nextIndex;
        return;
    }
    case HotkeyAction::EnterGroupMode: {
        if (m_isInGroupWindowMode) {
            auto count = m_model->groupCount();
            if (count <= 0) return;
            auto next = (lv->currentIndex().row() + 1) % count;
            lv->setCurrentIndex(m_model->index(next));
        } else {
            enterGroupWindowMode();
        }
        return;
    }
    case HotkeyAction::MoveSelectionUp: {
        if (auto index = lv->currentIndex(); index.isValid()) {
            auto center = lv->visualRect(index).center();
            auto wheelEvent = new QWheelEvent(center, lv->mapToGlobal(center), {},
                                              {120, 0},
                                              Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
            QApplication::postEvent(lv, wheelEvent);
        }
        return;
    }
    case HotkeyAction::MoveSelectionDown: {
        if (auto index = lv->currentIndex(); index.isValid()) {
            auto center = lv->visualRect(index).center();
            auto wheelEvent = new QWheelEvent(center, lv->mapToGlobal(center), {},
                                              {-120, 0},
                                              Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
            QApplication::postEvent(lv, wheelEvent);
        }
        return;
    }
    case HotkeyAction::ActivateSelected: {
        m_stayOpenMode = false;
        activateCurrentAndHide();
        return;
    }
    case HotkeyAction::DismissSwitcher: {
        m_stayOpenMode = false;
        hide();
        return;
    }
    default:
        return;
    }
}

void Widget::handleGlobalAction(HotkeyAction action, Qt::KeyboardModifiers modifiers) {
    qInfo() << "[Action] ENTER" << hotkeyActionName(action)
            << "state=" << (int)m_overlayState
            << "visible=" << isVisible();
    bool visible = isVisible();

    switch (action) {
    case HotkeyAction::ShowSwitcher:
        if (!visible) {
            qInfo() << "[Action] ShowSwitcher -> requestShow";
            requestShow();
        } else {
            // Overlay visible: Alt+Tab acts as CycleForward
            qInfo() << "[Action] ShowSwitcher -> overlay visible -> CycleForward";
            handleOverlayAction(HotkeyAction::CycleForward, modifiers);
        }
        return;
    case HotkeyAction::EnterGroupMode:
        if (visible && !isMinimized()) {
            qInfo() << "[Action] EnterGroupMode -> overlay action";
            handleOverlayAction(HotkeyAction::EnterGroupMode, modifiers);
        } else {
            // Not visible or minimized: cycle foreground app windows
            qInfo() << "[Action] EnterGroupMode -> cycle foreground windows";
            auto foreWin = GetForegroundWindow();
            auto& order = m_wm->groupWindowOrder();
            if (order.isEmpty()) {
                auto targetExe = Util::getWindowProcessPath(foreWin);
                order = m_wm->buildGroupWindowOrder(targetExe);
            }
            bool forward = !(modifiers & Qt::ShiftModifier);
            if (auto nextWin = WindowManager::rotateWindowInGroup(order, foreWin, forward)) {
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

void Widget::handleListItemClicked(const QModelIndex& index) {
    qInfo() << "[Click] item row=" << index.row() << "groupMode=" << m_isInGroupWindowMode
             << "stayOpen=" << m_stayOpenMode << "mouseClickActivate=" << cfg().getMouseClickActivateEnabled();

    if (!cfg().getMouseClickActivateEnabled() && !m_stayOpenMode) {
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
            Util::switchToWindow(group.windows.first().hwnd, true);
        }
        hide();
    }
}
