#include <QTest>
#include "WindowFilter.h"
#include "WindowTypes.h"

class TestWindowFilter : public QObject {
    Q_OBJECT

private slots:
    void testEmptyRulesAllowsAll();
    void testBlockByTitle();
    void testBlockByClassName();
    void testBlockByProcessName();
    void testBlockByProcessPath();
    void testMultipleRules();
    void testBuiltinRules();
    void testFilter();
};

void TestWindowFilter::testEmptyRulesAllowsAll() {
    WindowFilter filter;
    WindowDescriptor desc{.title = "Test Window", .className = "TestClass",
                          .processPath = R"(C:\test\app.exe)", .processName = "app.exe"};
    QVERIFY(filter.isAllowed(desc));
}

void TestWindowFilter::testBlockByTitle() {
    WindowFilter filter;
    WindowFilterRule rule;
    rule.rules.append({.title = "blocked"});
    filter.setRules(rule);
    WindowDescriptor blocked{.title = "Blocked Window"};
    WindowDescriptor allowed{.title = "Allowed Window"};
    QVERIFY(!filter.isAllowed(blocked));
    QVERIFY(filter.isAllowed(allowed));
}

void TestWindowFilter::testBlockByClassName() {
    WindowFilter filter;
    WindowFilterRule rule;
    rule.rules.append({.className = "Progman"});
    filter.setRules(rule);
    WindowDescriptor blocked{.className = "Progman"};
    WindowDescriptor allowed{.className = "NotProgman"};
    QVERIFY(!filter.isAllowed(blocked));
    QVERIFY(filter.isAllowed(allowed));
}

void TestWindowFilter::testBlockByProcessName() {
    WindowFilter filter;
    WindowFilterRule rule;
    rule.rules.append({.processName = "notepad.exe"});
    filter.setRules(rule);
    WindowDescriptor blocked{.processPath = R"(C:\Windows\notepad.exe)", .processName = "notepad.exe"};
    WindowDescriptor allowed{.processPath = R"(C:\Windows\calc.exe)", .processName = "calc.exe"};
    QVERIFY(!filter.isAllowed(blocked));
    QVERIFY(filter.isAllowed(allowed));
}

void TestWindowFilter::testBlockByProcessPath() {
    WindowFilter filter;
    WindowFilterRule rule;
    rule.rules.append({.processPath = R"(C:\Windows\System32\)"});
    filter.setRules(rule);
    WindowDescriptor blocked{.processPath = R"(C:\Windows\System32\cmd.exe)"};
    WindowDescriptor allowed{.processPath = R"(C:\Program Files\app.exe)"};
    QVERIFY(!filter.isAllowed(blocked));
    QVERIFY(filter.isAllowed(allowed));
}

void TestWindowFilter::testMultipleRules() {
    WindowFilter filter;
    WindowFilterRule rule;
    rule.rules.append({.title = "secret"});
    rule.rules.append({.className = "HiddenClass"});
    filter.setRules(rule);
    QVERIFY(!filter.isAllowed({.title = "secret window"}));
    QVERIFY(!filter.isAllowed({.className = "HiddenClass"}));
    QVERIFY(filter.isAllowed({.title = "normal", .className = "NormalClass"}));
}

void TestWindowFilter::testBuiltinRules() {
    auto rules = WindowFilter::builtinRules();
    QVERIFY(!rules.rules.isEmpty());
    QVERIFY(rules.rules.size() > 5);
    WindowFilter filter;
    filter.setRules(rules);
    QVERIFY(!filter.isAllowed({.className = "Progman"}));
}

void TestWindowFilter::testFilter() {
    WindowFilter filter;
    WindowFilterRule rule;
    rule.rules.append({.className = "Banned"});
    filter.setRules(rule);
    QList<WindowDescriptor> windows;
    windows.append({.title = "Good1", .className = "Normal"});
    windows.append({.title = "Bad", .className = "Banned"});
    windows.append({.title = "Good2", .className = "Normal"});
    auto result = filter.filter(windows);
    QCOMPARE(result.size(), 2);
    QCOMPARE(result[0].title, QString("Good1"));
    QCOMPARE(result[1].title, QString("Good2"));
}

QTEST_APPLESS_MAIN(TestWindowFilter)
#include "test_filter_main.moc"
