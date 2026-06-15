#ifndef WIN_SWITCHER_CONFIGMANAGER_H
#define WIN_SWITCHER_CONFIGMANAGER_H

#include <QApplication>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include "ConfigManagerBase.h"
#include "lifecycle/Logger.h"
#include "core/HotkeyAction.h"

enum DisplayMonitor {
    PrimaryMonitor,
    MouseMonitor,
    EnumCount
};

struct BlockedWindowEntry {
    bool enabled = true;
    QString comment;
    QString title;
    QString className;
    QString processName;
    QString processPath;
};

class ConfigManager : public ConfigManagerBase {
    inline static const QString FileName = "config.json";

public:
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    static ConfigManager& instance() {
        auto configDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(configDir);
        auto filePath = configDir + "/" + FileName;

        // Migrate from old config in app directory
        auto oldPath = QApplication::applicationDirPath() + "/" + FileName;
        if (QFile::exists(oldPath) && !QFile::exists(filePath)) {
            QFile::copy(oldPath, filePath);
            QFile::remove(oldPath);
        }

        static ConfigManager instance{filePath};
        return instance;
    }

public:
    DisplayMonitor getDisplayMonitor() {
        auto monitor = get("DisplayMonitor", DisplayMonitor::MouseMonitor).toInt();
        if (monitor < 0 || monitor >= DisplayMonitor::EnumCount) {
            qWarning() << "Invalid DisplayMonitor enum" << monitor;
            monitor = DisplayMonitor::MouseMonitor;
        }
        return static_cast<DisplayMonitor>(monitor);
    }

    void setDisplayMonitor(DisplayMonitor monitor) {
        set("DisplayMonitor", monitor);
    }

    QString getLanguage() {
        return get("Language", "en").toString();
    }

    void setLanguage(const QString& lang) {
        set("Language", lang);
    }

    bool getAlwaysRunAsAdmin() {
        return get("AlwaysRunAsAdmin", false).toBool();
    }

    void setAlwaysRunAsAdmin(bool enabled) {
        set("AlwaysRunAsAdmin", enabled);
    }

    bool getLetterJumpEnabled() {
        return get("LetterJumpEnabled", false).toBool();
    }

    void setLetterJumpEnabled(bool enabled) {
        set("LetterJumpEnabled", enabled);
    }

    int getTheme() {
        return get("Theme", 0).toInt();
    }

    void setTheme(int theme) {
        set("Theme", theme);
    }

    bool getPaused() {
        return get("Paused", false).toBool();
    }

    void setPaused(bool paused) {
        set("Paused", paused);
    }

    int getMinIconSize() {
        return get("MinIconSize", 32).toInt();
    }

    void setMinIconSize(int size) {
        set("MinIconSize", size);
    }

    bool getMouseClickActivateEnabled() {
        return get("MouseClickActivateEnabled", true).toBool();
    }

    void setMouseClickActivateEnabled(bool enabled) {
        set("MouseClickActivateEnabled", enabled);
    }

    bool getClickShowGroupForMultiWindow() {
        return get("ClickShowGroupForMultiWindow", true).toBool();
    }

    void setClickShowGroupForMultiWindow(bool enabled) {
        set("ClickShowGroupForMultiWindow", enabled);
    }

    Util::LogLevel getLogLevel() {
        auto val = get("LogLevel", static_cast<int>(Util::LogLevel::Info)).toInt();
        if (val < 0 || val >= static_cast<int>(Util::LogLevel::None))
            val = static_cast<int>(Util::LogLevel::Info);
        return static_cast<Util::LogLevel>(val);
    }

    void setLogLevel(Util::LogLevel level) {
        set("LogLevel", static_cast<int>(level));
    }

    QString getLogDirectory() {
        return get("LogDirectory", "").toString();
    }

    void setLogDirectory(const QString& path) {
        set("LogDirectory", path);
    }

    bool getIconCacheEnabled() {
        return get("IconCacheEnabled", true).toBool();
    }

    void setIconCacheEnabled(bool enabled) {
        set("IconCacheEnabled", enabled);
    }

    QString getIconCacheDirectory() {
        auto dir = get("IconCacheDirectory", "").toString();
        if (dir.isEmpty())
            return QApplication::applicationDirPath() + "/icon_cache";
        return dir;
    }

    void setIconCacheDirectory(const QString& path) {
        set("IconCacheDirectory", path);
    }

    // BlockedWindows stored as JSON array
    QList<BlockedWindowEntry> getBlockedWindows() {
        QList<BlockedWindowEntry> list;
        QJsonArray arr = m_root["BlockedWindows"].toArray();
        for (const auto& val : arr) {
            QJsonObject obj = val.toObject();
            BlockedWindowEntry entry;
            entry.enabled = obj.value("enabled").toBool(true);
            entry.comment = obj["comment"].toString();
            entry.title = obj["title"].toString();
            entry.className = obj["class"].toString();
            entry.processName = obj["processName"].toString();
            entry.processPath = obj["processPath"].toString();
            if (!entry.title.isEmpty() || !entry.className.isEmpty() ||
                !entry.processName.isEmpty() || !entry.processPath.isEmpty())
                list.append(entry);
        }
        return list;
    }

    void setBlockedWindows(const QList<BlockedWindowEntry>& list) {
        QJsonArray arr;
        for (const auto& entry : list) {
            QJsonObject obj;
            obj["enabled"] = entry.enabled;
            obj["comment"] = entry.comment;
            obj["title"] = entry.title;
            obj["class"] = entry.className;
            obj["processName"] = entry.processName;
            obj["processPath"] = entry.processPath;
            arr.append(obj);
        }
        m_root["BlockedWindows"] = arr;
        m_dirty = true;
    }

    HotkeyBindings getHotkeyBindings() {
        HotkeyBindings result;
        QJsonObject hotkeys = m_root["Hotkeys"].toObject();

        for (auto it = hotkeys.begin(); it != hotkeys.end(); ++it) {
            auto action = hotkeyActionFromName(it.key());
            if (hotkeyActionName(action) != it.key())
                continue;
            QJsonArray arr = it.value().toArray();
            QList<HotkeyBinding> bindings;
            for (const auto& val : arr) {
                HotkeyBinding b;
                if (val.isString()) {
                    b = HotkeyBinding::fromString(val.toString());
                } else if (val.isObject()) {
                    b = HotkeyBinding::fromJson(val.toObject());
                }
                if (b.isValid())
                    bindings.append(b);
            }
            result[action] = bindings;
        }
        return result;
    }

    void setHotkeyBindings(const HotkeyBindings& bindings) {
        QJsonObject hotkeys;
        for (auto it = bindings.begin(); it != bindings.end(); ++it) {
            QJsonArray arr;
            for (const auto& b : it.value()) {
                if (b.isValid())
                    arr.append(b.toJson());
            }
            hotkeys[hotkeyActionName(it.key())] = arr;
        }
        m_root["Hotkeys"] = hotkeys;
        m_dirty = true;
        qInfo() << "[Config] Saved hotkey bindings to JSON";
    }

    HotkeyBindings effectiveHotkeyBindings() {
        auto bindings = getHotkeyBindings();
        normalizeHotkeyBindings(bindings);
        for (auto action : AllActions)
            ensureRequiredBindings(action, bindings[action]);
        return bindings;
    }

    void resetHotkeys() {
        setHotkeyBindings(defaultHotkeyBindings());
        sync();
    }

private:
    explicit ConfigManager(const QString& filename) : ConfigManagerBase(filename) {}
};

inline ConfigManager& cfg() { return ConfigManager::instance(); }

#endif //WIN_SWITCHER_CONFIGMANAGER_H
