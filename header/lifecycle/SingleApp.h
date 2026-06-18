#ifndef WIN_SWITCHER_SINGLEAPP_H
#define WIN_SWITCHER_SINGLEAPP_H

#include <QSharedMemory>
#include <QDebug>

class SingleApp {
private:
    QSharedMemory sharedMemory;
    const QString m_key;

    void cleanupStale() {
        if (sharedMemory.attach()) {
            sharedMemory.detach();
            qInfo() << "SingleApp: stale shared memory cleaned up for key" << m_key;
        }
    }

public:
    explicit SingleApp(const QString& key) : sharedMemory(key), m_key(key) {
        cleanupStale();
    }

    ~SingleApp() {
        if (sharedMemory.isAttached()) {
            sharedMemory.detach();
            qInfo() << "SingleApp: shared memory detached for key" << m_key;
        }
    }

    /// check if another instance is running
    bool isRunning() {
        if (sharedMemory.attach()) { // sharedMemory exists
            sharedMemory.detach();
            qInfo() << "SingleApp: another instance is running";
            return true;
        }

        if (sharedMemory.create(1)) { // create sharedMemory (1 byte)
            qInfo() << "SingleApp: shared memory created";
            return false;
        } else {
            qWarning() << "SingleApp: create failed" << sharedMemory.errorString();
            return true;
        }
    }
};

#endif //WIN_SWITCHER_SINGLEAPP_H
