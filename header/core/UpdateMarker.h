#ifndef WIN_SWITCHER_UPDATEMARKER_H
#define WIN_SWITCHER_UPDATEMARKER_H

#include <QString>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>

namespace UpdateMarker {

inline QString dataDir() {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

inline QString markerPath() {
    return dataDir() + "/update_marker.txt";
}

inline QString backupRoot() {
    return dataDir() + "/backup";
}

inline QString rollbackCountPath() {
    return dataDir() + "/rollback_count.txt";
}

inline QString read() {
    QFile f(markerPath());
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(f.readAll()).trimmed();
}

inline bool write(const QString& status) {
    QFile f(markerPath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    f.write(status.toUtf8());
    return true;
}

inline bool removeMarker() {
    return QFile::remove(markerPath());
}

inline int readRollbackCount() {
    QFile f(rollbackCountPath());
    if (!f.open(QIODevice::ReadOnly))
        return 0;
    return QString::fromUtf8(f.readAll()).trimmed().toInt();
}

inline void writeRollbackCount(int count) {
    QFile f(rollbackCountPath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(QString::number(count).toUtf8());
}

inline void resetRollbackCount() {
    QFile::remove(rollbackCountPath());
}

inline bool hasBackup() {
    QDir dir(backupRoot());
    return dir.exists() && dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot).size() > 0;
}

inline QString latestBackupDir() {
    QDir dir(backupRoot());
    auto entries = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed);
    if (entries.isEmpty())
        return {};
    return dir.absoluteFilePath(entries.first());
}

inline bool cleanupBackup() {
    QString root = backupRoot();
    if (!QDir(root).exists())
        return true;
    if (QDir(root).removeRecursively()) {
        qInfo() << "[UpdateMarker] Backup cleaned up:" << root;
        return true;
    }
    qWarning() << "[UpdateMarker] Failed to clean up backup:" << root;
    return false;
}

} // namespace UpdateMarker

#endif //WIN_SWITCHER_UPDATEMARKER_H
