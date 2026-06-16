#include <QTest>

class TestWindowFilter;
class TestHotkey;

int main(int argc, char** argv) {
    int status = 0;
    TestWindowFilter twf;
    status |= QTest::qExec(&twf, argc, argv);
    TestHotkey th;
    status |= QTest::qExec(&th, argc, argv);
    return status;
}
