#ifndef WIN_SWITCHER_CONFIGMANAGERBASE_H
#define WIN_SWITCHER_CONFIGMANAGERBASE_H

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QFile>
#include <QDir>
#include <QDebug>

class ConfigManagerBase : public QObject {
    Q_OBJECT

protected:
    QJsonObject m_root;
    QString m_filePath;
    QProcess proc_editor;
    bool m_dirty = false;
    bool m_destroying = false;

public:
    ~ConfigManagerBase() override { m_destroying = true; sync(); }

    ConfigManagerBase(const ConfigManagerBase&) = delete;
    ConfigManagerBase& operator=(const ConfigManagerBase&) = delete;

    QVariant get(const QString& key, const QVariant& defaultValue = QVariant()) const;
    void set(const QString& key, const QVariant& value);
    void remove(const QString& key);

    void sync();

    void editConfigFile();

    QJsonObject& root() { return m_root; }
    const QJsonObject& root() const { return m_root; }

signals:
    void configEdited();

protected:
    explicit ConfigManagerBase(const QString& filepath);

    void load();
    void save() const;

private:
    static QJsonValue navigatePath(const QJsonObject& obj, const QStringList& parts);
    static void setPath(QJsonObject& obj, const QStringList& parts, const QJsonValue& value);
    static void removePath(QJsonObject& obj, const QStringList& parts);
};

#endif //WIN_SWITCHER_CONFIGMANAGERBASE_H
