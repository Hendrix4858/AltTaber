#ifndef WIN_SWITCHER_SINGLEAPP_H
#define WIN_SWITCHER_SINGLEAPP_H

#include <QSharedMemory>
#include <QDebug>

class SingleApp {
private:
    QSharedMemory sharedMemory;
    const QString m_key;

public:
    explicit SingleApp(const QString& key)
        : sharedMemory(key),
          m_key(key) {
    }

    SingleApp(const SingleApp&) = delete;
    SingleApp& operator=(const SingleApp&) = delete;

    ~SingleApp() {
        if (sharedMemory.isAttached()) {
            sharedMemory.detach();
            qInfo() << "SingleApp: shared memory detached for key" << m_key;
        }
    }

    /// Check existing instance and acquire lock if none exists
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
