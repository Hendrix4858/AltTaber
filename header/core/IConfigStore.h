#ifndef WIN_SWITCHER_ICONFIGSTORE_H
#define WIN_SWITCHER_ICONFIGSTORE_H

#include <QVariant>
#include <QString>

class QObject;

class IConfigStore {
public:
    virtual ~IConfigStore() = default;

    virtual QVariant get(const QString& key, const QVariant& defaultValue = QVariant()) const = 0;
    virtual void set(const QString& key, const QVariant& value) = 0;
    virtual void remove(const QString& key) = 0;
    virtual void sync() = 0;

    virtual QObject* signalObject() = 0;
};

#endif //WIN_SWITCHER_ICONFIGSTORE_H
