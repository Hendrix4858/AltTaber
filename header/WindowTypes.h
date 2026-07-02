#ifndef WIN_SWITCHER_WINDOWTYPES_H
#define WIN_SWITCHER_WINDOWTYPES_H

#include <Windows.h>
#include <QString>
#include <QIcon>
#include <QList>
#include <QSet>
#include <QDebug>
#include <QDateTime>
#include <QMetaType>

enum class WindowKind {
    Normal,
    Pwa,
};

struct AppIdentity {
    QString host;
    QString instance;
    QString iconKey;

    QString groupKey() const {
        return instance.isEmpty() ? host : host + QStringLiteral("::") + instance;
    }

    bool operator==(const AppIdentity& o) const {
        return host == o.host && instance == o.instance;
    }
    bool operator!=(const AppIdentity& o) const {
        return !(*this == o);
    }
};

inline QDebug operator<<(QDebug dbg, const AppIdentity& id) {
    dbg.nospace() << "AppIdentity(" << id.host << "::" << id.instance << ")";
    return dbg.space();
}

struct WindowInfo {
    QString title;
    QString className;
    QString appUserModelId;
    HWND hwnd = nullptr;
    WindowKind windowKind = WindowKind::Normal;
    QString pwaDisplayName;
    AppIdentity identity;
};

inline QDebug operator<<(QDebug dbg, const WindowInfo& info) {
    dbg.nospace() << "WindowInfo(" << info.title << ", " << info.className << ", " << info.hwnd << ")";
    return dbg.space();
}

Q_DECLARE_METATYPE(WindowInfo)

struct WindowDescriptor {
    QString title;
    QString className;
    QString processPath;
    QString processName;
    QString appUserModelId;
    HWND hwnd = nullptr;
    WindowKind windowKind = WindowKind::Normal;
    QString pwaDisplayName;
    AppIdentity identity;
};

inline QDebug operator<<(QDebug dbg, const WindowDescriptor& desc) {
    dbg.nospace() << "WindowDescriptor(" << desc.title << ", " << desc.className
                  << ", " << desc.processName << ", " << desc.hwnd << ")";
    return dbg.space();
}

struct WindowGroup {
    WindowGroup() = default;

    void addWindow(const WindowInfo& window) {
        windows.append(window);
    }

    QString exePath;
    QIcon icon;
    QList<WindowInfo> windows;

    QString displayName;

    QSet<QChar> fileDescriptionTokens;
    QSet<QChar> pwaNameTokens;
    QSet<QChar> titleTokens;
    QSet<QChar> displayNameTokens;
    QSet<QChar> processNameTokens;
    QSet<QChar> allJumpTokens;
};

Q_DECLARE_METATYPE(WindowGroup)

#endif //WIN_SWITCHER_WINDOWTYPES_H
