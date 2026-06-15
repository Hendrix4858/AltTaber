#include "core/ConfigManagerBase.h"
#include <QJsonDocument>
#include <QJsonValue>

ConfigManagerBase::ConfigManagerBase(const QString& filepath)
    : m_filePath(filepath) {
    if (QFile::exists(filepath))
        load();

    connect(&proc_editor, &QProcess::finished, this, [this] {
        qDebug() << "#Config file edit finished";
        load();
        emit configEdited();
    });
}

void ConfigManagerBase::load() {
    QFile file(m_filePath);
    if (!file.exists()) {
        m_root = QJsonObject();
        m_dirty = true;
        return;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open config file:" << m_filePath;
        m_root = QJsonObject();
        return;
    }
    QByteArray data = file.readAll();
    file.close();

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "Failed to parse config JSON:" << err.errorString();
        m_root = QJsonObject();
        return;
    }
    m_root = doc.object();
    m_dirty = false;
}

void ConfigManagerBase::save() const {
    QFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to write config file:" << m_filePath;
        return;
    }
    QJsonDocument doc(m_root);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
}

void ConfigManagerBase::sync() {
    if (m_dirty) {
        save();
        m_dirty = false;
        if (!m_destroying)
            emit configEdited();
    }
}

QVariant ConfigManagerBase::get(const QString& key, const QVariant& defaultValue) const {
    auto parts = key.split('/', Qt::SkipEmptyParts);
    if (parts.isEmpty()) return defaultValue;

    QJsonValue val = navigatePath(m_root, parts);
    if (val.isUndefined() || val.isNull())
        return defaultValue;
    return val.toVariant();
}

void ConfigManagerBase::set(const QString& key, const QVariant& value) {
    auto parts = key.split('/', Qt::SkipEmptyParts);
    if (parts.isEmpty()) return;

    setPath(m_root, parts, QJsonValue::fromVariant(value));
    m_dirty = true;
}

void ConfigManagerBase::remove(const QString& key) {
    auto parts = key.split('/', Qt::SkipEmptyParts);
    if (parts.isEmpty()) return;

    removePath(m_root, parts);
    m_dirty = true;
}

void ConfigManagerBase::editConfigFile() {
    if (proc_editor.state() == QProcess::Running) {
        qDebug() << "Editor is running";
        return;
    }

    // Ensure file exists
    sync();
    if (!QFile::exists(m_filePath)) {
        QFile file(m_filePath);
        file.open(QIODevice::WriteOnly);
        file.write("{}");
        file.close();
    }

    qDebug() << "#Editing config file" << m_filePath;
    proc_editor.start("notepad", {m_filePath});
}

QJsonValue ConfigManagerBase::navigatePath(const QJsonObject& obj, const QStringList& parts) {
    QJsonValue val = obj;
    for (const auto& part : parts) {
        if (!val.isObject()) return {};
        val = val.toObject().value(part);
    }
    return val;
}

void ConfigManagerBase::setPath(QJsonObject& obj, const QStringList& parts, const QJsonValue& value) {
    if (parts.size() == 1) {
        obj[parts[0]] = value;
        return;
    }
    QJsonObject child = obj[parts[0]].toObject();
    setPath(child, parts.mid(1), value);
    obj[parts[0]] = child;
}

void ConfigManagerBase::removePath(QJsonObject& obj, const QStringList& parts) {
    if (parts.size() == 1) {
        obj.remove(parts[0]);
        return;
    }
    QJsonObject child = obj[parts[0]].toObject();
    removePath(child, parts.mid(1));
    obj[parts[0]] = child;
}
