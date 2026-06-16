#include <QTest>
#include <QJsonObject>
#include "core/ConfigManagerBase.h"
#include "core/ConfigManager.h"

class TestConfigManagerBase : public ConfigManagerBase {
public:
    TestConfigManagerBase() : ConfigManagerBase(":memory:") {}
    QJsonObject& testRoot() { return m_root; }
};

class TestConfig : public QObject {
    Q_OBJECT

private slots:
    void testGetDefault();
    void testSetAndGet();
    void testNestedPaths();
    void testRemove();
    void testOverwrite();
    void testNonExistentKey();
};

void TestConfig::testGetDefault() {
    TestConfigManagerBase cfg;
    QCOMPARE(cfg.get("NonExistent", 42).toInt(), 42);
    QCOMPARE(cfg.get("MissingKey", "default").toString(), QString("default"));
}

void TestConfig::testSetAndGet() {
    TestConfigManagerBase cfg;
    cfg.set("Theme", 1);
    QCOMPARE(cfg.get("Theme", -1).toInt(), 1);

    cfg.set("Language", "zh_CN");
    QCOMPARE(cfg.get("Language", "").toString(), QString("zh_CN"));
}

void TestConfig::testNestedPaths() {
    TestConfigManagerBase cfg;
    cfg.set("Hotkeys/SwitchToNextWindow", "Alt+Tab");
    QCOMPARE(cfg.get("Hotkeys/SwitchToNextWindow", "").toString(), QString("Alt+Tab"));

    cfg.set("Display/Monitor", 0);
    cfg.set("Display/Theme", 2);
    QCOMPARE(cfg.get("Display/Monitor", -1).toInt(), 0);
    QCOMPARE(cfg.get("Display/Theme", -1).toInt(), 2);
}

void TestConfig::testRemove() {
    TestConfigManagerBase cfg;
    cfg.set("KeyToRemove", "value");
    QVERIFY(cfg.get("KeyToRemove", "").toString() == "value");
    cfg.remove("KeyToRemove");
    QCOMPARE(cfg.get("KeyToRemove", "notfound").toString(), QString("notfound"));

    cfg.set("Nested/Child", 42);
    cfg.remove("Nested/Child");
    QCOMPARE(cfg.get("Nested/Child", -1).toInt(), -1);
}

void TestConfig::testOverwrite() {
    TestConfigManagerBase cfg;
    cfg.set("Key", "old");
    cfg.set("Key", "new");
    QCOMPARE(cfg.get("Key", "").toString(), QString("new"));

    cfg.set("Count", 1);
    cfg.set("Count", 99);
    QCOMPARE(cfg.get("Count", 0).toInt(), 99);
}

void TestConfig::testNonExistentKey() {
    TestConfigManagerBase cfg;
    QVERIFY(cfg.get("DoesNotExist").isNull());
    QCOMPARE(cfg.get("", "fallback").toString(), QString("fallback"));
}

#include "test_config.moc"


