#include "core/ConfigManager.h"

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTemporaryFile>

namespace {

constexpr auto kConfigFileName = "config.json";
constexpr auto kLocationFileName = "_config_location.json";

QString appDataConfigDir() {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

QString typeString(ConfigLocation location) {
    switch (location) {
        case ConfigLocation::ProgramDir:
            return "program";
        case ConfigLocation::Custom:
            return "custom";
        case ConfigLocation::AppData:
            break;
    }
    return "appdata";
}

ConfigLocation locationFromType(const QString& type) {
    if (type == "program") return ConfigLocation::ProgramDir;
    if (type == "custom") return ConfigLocation::Custom;
    return ConfigLocation::AppData;
}

bool isDirWritable(const QString& dir) {
    QDir d(dir);
    if (!d.exists() && !d.mkpath(dir))
        return false;
    QTemporaryFile probe(d.filePath("config_write_test_XXXXXX"));
    return probe.open();
}

QJsonObject loadJsonFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
        return {};
    return doc.object();
}

bool writeJsonFile(const QString& path, const QJsonObject& obj) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly))
        return false;
    out.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    return out.commit();
}

bool writeLocationMarker(const ConfigLocationInfo& info) {
    QJsonObject obj;
    obj["type"] = typeString(info.location);
    if (info.location == ConfigLocation::Custom)
        obj["path"] = info.customPath;
    return writeJsonFile(ConfigManager::configLocationFilePath(), obj);
}

void deepMerge(QJsonObject& base, const QJsonObject& overlay) {
    for (auto it = overlay.begin(); it != overlay.end(); ++it) {
        auto baseIt = base.find(it.key());
        if (baseIt != base.end() && baseIt->isObject() && it->isObject()) {
            QJsonObject child = baseIt->toObject();
            deepMerge(child, it->toObject());
            base[it.key()] = child;
        } else {
            base[it.key()] = it.value();
        }
    }
}

ConfigLocationInfo resolveInitialLocation() {
    // An explicit user choice already persisted: always respect it.
    if (QFile::exists(ConfigManager::configLocationFilePath()))
        return ConfigManager::readConfigLocationInfo();

    // No marker yet: probe existing configs first so existing users keep
    // their settings instead of being silently switched to a new location.
    if (QFile::exists(QApplication::applicationDirPath() + "/" + kConfigFileName))
        return {ConfigLocation::ProgramDir, {}};  // portable setup

    if (QFile::exists(ConfigManager::configFilePathFor(ConfigLocation::AppData, {})))
        return {ConfigLocation::AppData, {}};     // legacy AppData config

    return {ConfigLocation::ProgramDir, {}};      // fresh install: program dir
}

} // namespace

QString ConfigManager::configLocationFilePath() {
    return appDataConfigDir() + "/" + kLocationFileName;
}

ConfigLocationInfo ConfigManager::readConfigLocationInfo() {
    ConfigLocationInfo info;
    QFile file(configLocationFilePath());
    if (file.open(QIODevice::ReadOnly)) {
        QJsonObject obj = QJsonDocument::fromJson(file.readAll()).object();
        info.location = locationFromType(obj["type"].toString());
        info.customPath = obj["path"].toString();
    }
    return info;
}

QString ConfigManager::configFilePathFor(ConfigLocation location, const QString& customPath) {
    switch (location) {
        case ConfigLocation::ProgramDir:
            return QApplication::applicationDirPath() + "/" + kConfigFileName;
        case ConfigLocation::Custom:
            if (!customPath.isEmpty())
                return QDir::cleanPath(customPath) + "/" + kConfigFileName;
            break;
        case ConfigLocation::AppData:
            break;
    }
    return appDataConfigDir() + "/" + kConfigFileName;
}

ConfigManager& ConfigManager::instance() {
    static ConfigManager instance = [] {
        auto info = resolveInitialLocation();
        auto filePath = configFilePathFor(info.location, info.customPath);

        // The chosen directory is not writable (e.g. Program Files): fall back
        // to AppData so settings can always be saved.
        if (!isDirWritable(QFileInfo(filePath).absolutePath())) {
            qWarning() << "[Config] Config directory not writable, falling back to AppData:"
                       << filePath;
            info.location = ConfigLocation::AppData;
            info.customPath.clear();
            filePath = configFilePathFor(info.location, info.customPath);
            writeLocationMarker(info);
        }

        // First run: materialize the config file so the location is discoverable.
        if (!QFile::exists(filePath))
            writeJsonFile(filePath, {});

        return ConfigManager(filePath);
    }();
    return instance;
}

bool ConfigManager::setConfigLocation(ConfigLocation location, const QString& customPath) {
    if (location == ConfigLocation::Custom &&
        (customPath.trimmed().isEmpty() || !QFileInfo(customPath).isAbsolute()))
        return false;

    const QString newPath = configFilePathFor(location, customPath);
    const QString newDir = QFileInfo(newPath).absolutePath();

    if (newPath == m_filePath) {
        // Same file: just persist the user's choice.
        return writeLocationMarker({location, customPath});
    }

    if (!isDirWritable(newDir)) {
        qWarning() << "[Config] Target config directory is not writable:" << newDir;
        return false;
    }

    // Migrate: merge into an existing file at the target instead of overwriting it,
    // with the current settings taking precedence.
    QJsonObject target = loadJsonFile(newPath);
    deepMerge(target, m_root);

    if (!writeJsonFile(newPath, target)) {
        qWarning() << "[Config] Failed to migrate config to:" << newPath;
        return false;
    }

    if (!writeLocationMarker({location, customPath})) {
        qWarning() << "[Config] Failed to persist config location marker";
        return false;
    }

    const QString oldPath = m_filePath;
    m_filePath = newPath;
    m_root = target;
    m_dirty = false;

    if (oldPath != newPath && QFile::exists(oldPath)) {
        if (!QFile::remove(oldPath))
            qWarning() << "[Config] Failed to remove old config file:" << oldPath;
    }

    qInfo() << "[Config] Config migrated from" << oldPath << "to" << newPath;
    emit configEdited();
    return true;
}

ConfigLocationInfo ConfigManager::configLocationInfo() const {
    return readConfigLocationInfo();
}
