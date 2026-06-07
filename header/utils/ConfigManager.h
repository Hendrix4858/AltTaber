#ifndef WIN_SWITCHER_CONFIGMANAGER_H
#define WIN_SWITCHER_CONFIGMANAGER_H

#include <QSettings>
#include <QApplication>
#include "ConfigManagerBase.h"
#include "utils/Logger.h"

enum DisplayMonitor {
    PrimaryMonitor,
    MouseMonitor,
    EnumCount
};

struct BlockedWindowEntry {
    QString title;
    QString className;
};

class ConfigManager : public ConfigManagerBase {
    inline static const QString FileName = "config.ini";

public:
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    static ConfigManager& instance() {
        static const auto filePath = QApplication::applicationDirPath() + "/" + FileName;
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

    bool getStayOpenOnAltRelease() {
        return get("StayOpenOnAltRelease", false).toBool();
    }

    void setStayOpenOnAltRelease(bool enabled) {
        set("StayOpenOnAltRelease", enabled);
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

    QList<BlockedWindowEntry> getBlockedWindows() {
        QList<BlockedWindowEntry> list;
        int count = get("BlockedWindows/count", 0).toInt();
        for (int i = 0; i < count; ++i) {
            BlockedWindowEntry entry;
            entry.title = get(QString("BlockedWindows/%1_title").arg(i), "").toString();
            entry.className = get(QString("BlockedWindows/%1_class").arg(i), "").toString();
            if (!entry.title.isEmpty() || !entry.className.isEmpty())
                list.append(entry);
        }
        return list;
    }

    void setBlockedWindows(const QList<BlockedWindowEntry>& list) {
        remove("BlockedWindows");
        set("BlockedWindows/count", list.size());
        for (int i = 0; i < list.size(); ++i) {
            set(QString("BlockedWindows/%1_title").arg(i), list[i].title);
            set(QString("BlockedWindows/%1_class").arg(i), list[i].className);
        }
    }

private:
    explicit ConfigManager(const QString& filename) : ConfigManagerBase(filename) {}
};

inline ConfigManager& cfg() { return ConfigManager::instance(); }

#endif //WIN_SWITCHER_CONFIGMANAGER_H
