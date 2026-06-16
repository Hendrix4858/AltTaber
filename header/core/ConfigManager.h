#ifndef WIN_SWITCHER_CONFIGMANAGER_H
#define WIN_SWITCHER_CONFIGMANAGER_H

#include <QApplication>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include "ConfigManagerBase.h"
#include "utils/PathUtils.h"
#include "core/ThemeManager.h"
#include "lifecycle/Logger.h"
#include "core/HotkeyAction.h"

enum DisplayMonitor {
    PrimaryMonitor,
    MouseMonitor,
};
constexpr int DisplayMonitorCount = 2;

enum class PwaMode {
    SeparateGroups,
    TagWithinGroup,
};
constexpr int PwaModeCount = 2;

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

        static ConfigManager instance{filePath};
        return instance;
    }

public:
    DisplayMonitor getDisplayMonitor() {
        auto monitor = get("DisplayMonitor", DisplayMonitor::MouseMonitor).toInt();
        if (monitor < 0 || monitor >= DisplayMonitorCount) {
            qWarning() << "Invalid DisplayMonitor enum" << monitor;
            monitor = DisplayMonitor::MouseMonitor;
        }
        return static_cast<DisplayMonitor>(monitor);
    }

    void setDisplayMonitor(DisplayMonitor monitor) {
        set("DisplayMonitor", monitor);
    }

    QString getLanguage() {
        return get("Language", "system").toString();
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
        return get("Theme", System).toInt();
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

    bool getPwaEnabled() {
        return get("PwaEnabled", true).toBool();
    }

    void setPwaEnabled(bool enabled) {
        set("PwaEnabled", enabled);
    }

    PwaMode getPwaMode() {
        auto mode = get("PwaMode", static_cast<int>(PwaMode::SeparateGroups)).toInt();
        if (mode < 0 || mode >= PwaModeCount)
            mode = static_cast<int>(PwaMode::SeparateGroups);
        return static_cast<PwaMode>(mode);
    }

    void setPwaMode(PwaMode mode) {
        set("PwaMode", static_cast<int>(mode));
    }

    int getMinIconSize() {
        return get("MinIconSize", 32).toInt();
    }

    void setMinIconSize(int size) {
        set("MinIconSize", size);
    }

    bool getMouseClickActivateEnabled() {
        return get("MouseClickActivateEnabled", false).toBool();
    }

    void setMouseClickActivateEnabled(bool enabled) {
        set("MouseClickActivateEnabled", enabled);
    }

    bool getClickShowGroupForMultiWindow() {
        return get("ClickShowGroupForMultiWindow", false).toBool();
    }

    void setClickShowGroupForMultiWindow(bool enabled) {
        set("ClickShowGroupForMultiWindow", enabled);
    }

    Util::LogFlags getLogFlags() {
        if (!m_root.contains("LogFlags"))
            return Util::DEFAULT_LOG_FLAGS;
        auto arr = get("LogFlags").toJsonArray();
        Util::LogFlags flags = 0;
        for (const auto& v : arr) {
            auto name = v.toString();
            if (name == u"Debug")   flags |= Util::LogDebug;
            else if (name == u"Info")    flags |= Util::LogInfo;
            else if (name == u"Warning") flags |= Util::LogWarn;
            else if (name == u"Error")   flags |= Util::LogError;
            else if (name == u"Fatal")   flags |= Util::LogFatal;
        }
        return flags;
    }

    void setLogFlags(Util::LogFlags flags) {
        QJsonArray arr;
        if (flags & Util::LogDebug) arr.append("Debug");
        if (flags & Util::LogInfo)  arr.append("Info");
        if (flags & Util::LogWarn)  arr.append("Warning");
        if (flags & Util::LogError) arr.append("Error");
        if (flags & Util::LogFatal) arr.append("Fatal");
        set("LogFlags", arr);
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
        return PathUtils::resolveAppRelativePath(
            get("IconCacheDirectory", "").toString(), "icon_cache");
    }

    void setIconCacheDirectory(const QString& path) {
        set("IconCacheDirectory", path);
    }

    // BlockedWindows stored as JSON array
    QList<BlockedWindowEntry> getBlockedWindows() {
        QList<BlockedWindowEntry> list;
        QJsonArray arr = m_root.value("BlockedWindows").toArray();
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
        QJsonObject hotkeys = m_root.value("Hotkeys").toObject();

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
        if (!m_root.contains("Hotkeys")) {
            setHotkeyBindings(bindings);
            sync();
        }
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
