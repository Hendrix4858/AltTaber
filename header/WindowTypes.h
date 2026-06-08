#ifndef WIN_SWITCHER_WINDOWTYPES_H
#define WIN_SWITCHER_WINDOWTYPES_H

#include <Windows.h>
#include <QString>
#include <QIcon>
#include <QList>
#include <QDebug>
#include <QDateTime>
#include <QMetaType>

struct WindowInfo {
    QString title;
    QString className;
    HWND hwnd = nullptr;
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
    HWND hwnd = nullptr;
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
};

Q_DECLARE_METATYPE(WindowGroup)

#endif //WIN_SWITCHER_WINDOWTYPES_H
