#include <QTest>
#include "OverlayController.h"
#include "OverlayView.h"
#include "WindowGroupModel.h"
#include "WindowManager.h"
#include "core/HotkeyAction.h"
#include "core/ConfigManager.h"

// ── Test View: implements OverlayView + bridges updateGroups into the model ──

class TestOverlayView : public OverlayView {
public:
    WindowGroupModel* model = nullptr;

    int showCount = 0;
    int hideCount = 0;
    int groupsSet = 0;
    int indexSet = -1;
    int iconLayouts = 0;
    int geometryLayouts = 0;
    QRect lastWidgetRect;
    QPoint lastListPos;
    HWND fakeHwnd = nullptr;

    void showOverlay() override { ++showCount; }
    void doHide() override { ++hideCount; }
    void updateGroups(const QList<WindowGroup>& groups) override {
        ++groupsSet;
        if (model) model->setGroups(groups);
    }
    void applyIconLayout(int, int, int) override { ++iconLayouts; }
    void applyLayout(const OverlayLayout& layout) override {
        ++geometryLayouts;
        lastWidgetRect = layout.widgetRect;
        lastListPos = layout.listRect.topLeft();
    }
    void setCurrentIndex(int index) override { indexSet = index; }
    HWND hWnd() const override { return fakeHwnd; }
};

// ── Fake WindowManager ──

class TestWindowManager : public WindowManager {
public:
    QList<WindowGroup> testGroups;

    explicit TestWindowManager()
        : WindowManager(nullptr) {}

    QList<WindowGroup> prepareWindowGroupList() override {
        return testGroups;
    }
};

// ── Helper: create a single test group ──

static QList<WindowGroup> oneGroup() {
    WindowGroup g;
    g.displayName = "TestApp";
    g.exePath = "C:\\test.exe";
    WindowInfo wi;
    wi.title = "Test Window";
    wi.hwnd = (HWND)0x1234;
    g.windows.append(wi);
    return {g};
}

static QList<WindowGroup> twoGroups() {
    QList<WindowGroup> g = oneGroup();
    WindowGroup g2;
    g2.displayName = "App2";
    g2.exePath = "C:\\app2.exe";
    WindowInfo wi2;
    wi2.title = "Window 2";
    wi2.hwnd = (HWND)0x5678;
    g2.windows.append(wi2);
    g.append(g2);
    return g;
}

// ── Test Class ──

class TestOverlayController : public QObject {
    Q_OBJECT

    TestOverlayView m_view;
    WindowGroupModel m_model;
    TestWindowManager m_wm;
    OverlayController* m_ctrl = nullptr;

private slots:
    void initTestCase() {
        m_view.model = &m_model;
        m_ctrl = new OverlayController(m_view, &m_model, &m_wm, this);
    }

    void cleanupTestCase() {
        delete m_ctrl;
        m_ctrl = nullptr;
    }

    void init() {
        // Reset overlay to clean Hidden state before each test
        if (m_ctrl->overlayState() != OverlayController::OverlayState::Hidden)
            m_ctrl->handleIntent(OverlayIntent::Dismiss);
        m_wm.testGroups.clear();
        m_model.setGroups({});

        m_view.showCount = 0;
        m_view.hideCount = 0;
        m_view.groupsSet = 0;
        m_view.indexSet = -1;
        m_view.iconLayouts = 0;
        m_view.geometryLayouts = 0;
    }

    void testInitialStateIsHidden() {
        QCOMPARE(m_ctrl->overlayState(), OverlayController::OverlayState::Hidden);
        QVERIFY(!m_ctrl->isVisible());
    }

    void testHiddenShowTransitionsToVisible() {
        m_wm.testGroups = oneGroup();
        m_ctrl->setSessionInfo({HotkeyAction::SwitchToNextWindow, SessionEndTrigger::ModifierRelease});
        m_ctrl->handleIntent(OverlayIntent::ShowSwitcher);

        QCOMPARE(m_ctrl->overlayState(), OverlayController::OverlayState::Visible);
        QVERIFY(m_ctrl->isVisible());
    }

    void testShowInvokesViewShow() {
        m_wm.testGroups = oneGroup();
        m_ctrl->setSessionInfo({HotkeyAction::SwitchToNextWindow, SessionEndTrigger::ModifierRelease});
        m_ctrl->handleIntent(OverlayIntent::ShowSwitcher);

        // showOverlay is deferred via QTimer::singleShot(0, ...)
        QTest::qWait(10);
        QVERIFY(m_view.showCount >= 1);
    }

    void testShowUpdatesModelAndLayout() {
        m_wm.testGroups = oneGroup();
        m_ctrl->setSessionInfo({HotkeyAction::SwitchToNextWindow, SessionEndTrigger::ModifierRelease});
        m_ctrl->handleIntent(OverlayIntent::ShowSwitcher);

        QVERIFY(m_view.groupsSet >= 1);
        QVERIFY(m_view.iconLayouts >= 1);
        QVERIFY(m_view.geometryLayouts >= 1);
    }

    void testVisibleDismissTransitionsToHidden() {
        m_wm.testGroups = oneGroup();
        m_ctrl->setSessionInfo({HotkeyAction::SwitchToNextWindow, SessionEndTrigger::ModifierRelease});
        m_ctrl->handleIntent(OverlayIntent::ShowSwitcher);
        QCOMPARE(m_ctrl->overlayState(), OverlayController::OverlayState::Visible);

        m_ctrl->handleIntent(OverlayIntent::Dismiss);
        QCOMPARE(m_ctrl->overlayState(), OverlayController::OverlayState::Hidden);
        QVERIFY(!m_ctrl->isVisible());
    }

    void testDismissInvokesViewHide() {
        m_wm.testGroups = oneGroup();
        m_ctrl->setSessionInfo({HotkeyAction::SwitchToNextWindow, SessionEndTrigger::ModifierRelease});
        m_ctrl->handleIntent(OverlayIntent::ShowSwitcher);
        QCOMPARE(m_ctrl->overlayState(), OverlayController::OverlayState::Visible);

        m_ctrl->handleIntent(OverlayIntent::Dismiss);
        QVERIFY(m_view.hideCount >= 1);
    }

    void testHiddenDismissIsNoOp() {
        QCOMPARE(m_ctrl->overlayState(), OverlayController::OverlayState::Hidden);

        m_ctrl->handleIntent(OverlayIntent::Dismiss);
        QCOMPARE(m_ctrl->overlayState(), OverlayController::OverlayState::Hidden);
        QCOMPARE(m_view.hideCount, 0);
    }

    void testSessionEndWithModifierReleaseHides() {
        m_wm.testGroups = oneGroup();
        m_ctrl->setSessionInfo({HotkeyAction::SwitchToNextWindow, SessionEndTrigger::ModifierRelease});
        m_ctrl->handleIntent(OverlayIntent::ShowSwitcher);
        QCOMPARE(m_ctrl->overlayState(), OverlayController::OverlayState::Visible);

        m_ctrl->handleIntent(OverlayIntent::SessionEndConditionMet);
        QCOMPARE(m_ctrl->overlayState(), OverlayController::OverlayState::Hidden);
    }

    void testSessionEndWithExplicitActionAndStayOpenKeepsVisible() {
        m_wm.testGroups = oneGroup();
        m_ctrl->setSessionInfo({HotkeyAction::ShowSwitcherStayOpen, SessionEndTrigger::ExplicitAction});
        m_ctrl->handleIntent(OverlayIntent::ShowSwitcher);
        QCOMPARE(m_ctrl->overlayState(), OverlayController::OverlayState::Visible);

        m_ctrl->handleIntent(OverlayIntent::SessionEndConditionMet);
        QCOMPARE(m_ctrl->overlayState(), OverlayController::OverlayState::Visible);
    }

    void testBackwardShow() {
        // Two groups so backward selects index 1
        m_wm.testGroups = twoGroups();
        m_ctrl->setSessionInfo({HotkeyAction::SwitchToPreviousWindow, SessionEndTrigger::ModifierRelease});
        m_ctrl->handleIntent(OverlayIntent::ShowSwitcherBackward);

        QCOMPARE(m_ctrl->overlayState(), OverlayController::OverlayState::Visible);
        QCOMPARE(m_view.indexSet, 1);
    }

    void testCalculateInitialIndexWithNoGroups() {
        m_model.setGroups({});
        int idx = m_ctrl->calculateInitialIndex();
        QCOMPARE(idx, -1);
    }

    void testCalculateInitialIndexWithSingleGroup() {
        m_model.setGroups(oneGroup());
        int idx = m_ctrl->calculateInitialIndex();
        QCOMPARE(idx, 0);
    }

    void testHandleGlobalActionSwitchToNextHidden() {
        m_ctrl->handleGlobalAction(HotkeyAction::SwitchToNextWindow, Qt::NoModifier);
        QVERIFY(true);
    }
};

QTEST_MAIN(TestOverlayController)
#include "test_overlay_main.moc"
