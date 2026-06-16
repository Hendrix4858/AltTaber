#include <QTest>
#include "HotkeyConflictResolver.h"
#include "core/HotkeyAction.h"
#include "core/HotkeyBinding.h"

class TestHotkey : public QObject {
    Q_OBJECT

private slots:
    void testNoConflictWithEmpty();
    void testConflictSameBindingSameScope();
    void testNoConflictDifferentScope();
    void testMultipleBindingsOneConflicts();
    void testNoConflictWhenSameAction();
    void testHasSingleLetter();
    void testNoSingleLetter();
};

void TestHotkey::testNoConflictWithEmpty() {
    HotkeyBindings empty;
    HotkeyBinding binding = HotkeyBinding::fromString("Alt+Tab");
    auto result = HotkeyConflictResolver::findConflict(
        HotkeyAction::SwitchToNextWindow, binding, empty);
    QVERIFY(!result.found);
}

void TestHotkey::testConflictSameBindingSameScope() {
    HotkeyBinding binding = HotkeyBinding::fromString("Alt+Tab");
    HotkeyBindings bindings;
    bindings[HotkeyAction::SwitchToNextWindow].append(binding);
    bindings[HotkeyAction::SwitchToPreviousWindow].append(HotkeyBinding::fromString("Alt+Shift+Tab"));
    auto result = HotkeyConflictResolver::findConflict(
        HotkeyAction::ShowSwitcherStayOpen, binding, bindings);
    QVERIFY(result.found);
    QCOMPARE(result.action, HotkeyAction::SwitchToNextWindow);
}

void TestHotkey::testNoConflictDifferentScope() {
    HotkeyBinding tab = HotkeyBinding::fromString("Tab");
    HotkeyBindings bindings;
    bindings[HotkeyAction::CycleForward].append(tab);
    auto result = HotkeyConflictResolver::findConflict(
        HotkeyAction::SwitchToNextWindow, tab, bindings);
    QVERIFY(!result.found);
}

void TestHotkey::testMultipleBindingsOneConflicts() {
    HotkeyBindings bindings;
    bindings[HotkeyAction::SwitchToNextWindow].append(HotkeyBinding::fromString("Alt+Tab"));
    bindings[HotkeyAction::SwitchToPreviousWindow].append(HotkeyBinding::fromString("Alt+Shift+Tab"));
    auto result = HotkeyConflictResolver::findConflict(
        HotkeyAction::ShowSwitcherStayOpen, HotkeyBinding::fromString("Alt+Shift+Tab"), bindings);
    QVERIFY(result.found);
    QCOMPARE(result.action, HotkeyAction::SwitchToPreviousWindow);
}

void TestHotkey::testNoConflictWhenSameAction() {
    HotkeyBindings bindings;
    bindings[HotkeyAction::SwitchToNextWindow].append(HotkeyBinding::fromString("Alt+Tab"));
    auto result = HotkeyConflictResolver::findConflict(
        HotkeyAction::SwitchToNextWindow, HotkeyBinding::fromString("Alt+Tab"), bindings);
    QVERIFY(!result.found);
}

void TestHotkey::testHasSingleLetter() {
    HotkeyBindings bindings;
    bindings[HotkeyAction::SwitchToNextWindow].append(HotkeyBinding::fromString("Alt+Tab"));
    bindings[HotkeyAction::ActivateSelected].append(HotkeyBinding::fromString("A"));
    QVERIFY(HotkeyConflictResolver::hasSingleLetterBinding(bindings));
}

void TestHotkey::testNoSingleLetter() {
    HotkeyBindings bindings;
    bindings[HotkeyAction::SwitchToNextWindow].append(HotkeyBinding::fromString("Alt+Tab"));
    bindings[HotkeyAction::CycleForward].append(HotkeyBinding::fromString("Tab"));
    QVERIFY(!HotkeyConflictResolver::hasSingleLetterBinding(bindings));
}

QTEST_APPLESS_MAIN(TestHotkey)
#include "test_hotkey_main.moc"
