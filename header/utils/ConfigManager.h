#ifndef WIN_SWITCHER_CONFIGMANAGER_H
#define WIN_SWITCHER_CONFIGMANAGER_H

#include <QSettings>
#include <QApplication>
#include "ConfigManagerBase.h"

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
