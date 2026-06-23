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

    void testMatchesPhysicalVkOnly();
    void testMatchesPhysicalScanAndExt();
    void testMatchesPhysicalScanMismatch();
    void testMatchesPhysicalExtMismatch();
    void testMatchesPhysicalModsMismatch();
    void testMatchesPhysicalLogicalFallback();
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

void TestHotkey::testMatchesPhysicalVkOnly() {
    auto b = makePhysicalBinding(Qt::NoModifier, VK_UP, 0x48, true);
    QVERIFY(b.matchesPhysical(VK_UP, 0x48, true, Qt::NoModifier));
}

void TestHotkey::testMatchesPhysicalScanAndExt() {
    auto b = makePhysicalBinding(Qt::NoModifier, VK_UP, 0x48, true);
    QVERIFY(b.matchesPhysical(VK_UP, 0x48, true, Qt::NoModifier));
}

void TestHotkey::testMatchesPhysicalScanMismatch() {
    auto b = makePhysicalBinding(Qt::NoModifier, VK_UP, 0x48, true);
    QVERIFY(!b.matchesPhysical(VK_UP, 0x50, true, Qt::NoModifier));
}

void TestHotkey::testMatchesPhysicalExtMismatch() {
    auto b = makePhysicalBinding(Qt::NoModifier, VK_UP, 0x48, true);
    QVERIFY(!b.matchesPhysical(VK_UP, 0x48, false, Qt::NoModifier));
}

void TestHotkey::testMatchesPhysicalModsMismatch() {
    auto b = makePhysicalBinding(Qt::NoModifier, VK_UP, 0x48, true);
    QVERIFY(!b.matchesPhysical(VK_UP, 0x48, true, Qt::AltModifier));
}

void TestHotkey::testMatchesPhysicalLogicalFallback() {
    auto b = HotkeyBinding::fromString("Alt+Tab");
    QCOMPARE(b.matchesPhysical(VK_TAB, 0x0F, false, Qt::AltModifier),
             b.matches(VK_TAB, Qt::AltModifier));
    QVERIFY(b.matchesPhysical(VK_TAB, 0x0F, false, Qt::AltModifier));
}

QTEST_APPLESS_MAIN(TestHotkey)
#include "test_hotkey_main.moc"
