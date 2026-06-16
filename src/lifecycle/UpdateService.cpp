#include "lifecycle/UpdateService.h"
#include "core/UpdateMarker.h"
#include "core/QuitReason.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QDebug>

bool UpdateService::handleUpdateRollback() {
    auto marker = UpdateMarker::read();

    if (marker == "ok" && UpdateMarker::hasBackup()) {
        qInfo() << "[Update] Cleaning up stale backup (previous update ok)";
        UpdateMarker::cleanupBackup();
    }

    if (marker == "pending" && UpdateMarker::hasBackup()) {
        int attempts = UpdateMarker::readRollbackCount();
        UpdateMarker::writeRollbackCount(attempts + 1);
        if (attempts >= 1) {
            qWarning() << "[Update] Prior init attempt detected, initiating rollback";
            if (tryRollback())
                return true;
        } else {
            qInfo() << "[Update] First attempt after update, proceeding normally";
        }
    }
    return false;
}

void UpdateService::cleanupUpdateMarkers() {
    UpdateMarker::removeMarker();
    UpdateMarker::resetRollbackCount();
    if (UpdateMarker::hasBackup())
        UpdateMarker::cleanupBackup();
    qInfo() << "[Main] Update marker cleaned up, init complete";
}

bool UpdateService::tryRollback() {
    auto backupDir = UpdateMarker::latestBackupDir();
    if (backupDir.isEmpty()) {
        qWarning() << "[Rollback] No backup directory found";
        return false;
    }

    auto appDir = QCoreApplication::applicationDirPath();
    auto markerPath = UpdateMarker::markerPath();
    auto batPath = QDir::temp().absoluteFilePath("AltTaber_rollback.bat");

    QFile bat(batPath);
    if (!bat.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QString content = QString(
        "@echo off\r\n"
        "timeout /t 3 /nobreak >nul\r\n"
        "xcopy \"%1\\*.*\" \"%2\\\" /E /Y /I >nul 2>&1\r\n"
        "if exist \"%3\" del \"%3\" >nul 2>&1\r\n"
        "rmdir /S /Q \"%1\" >nul 2>&1\r\n"
        "start \"\" \"%2\\AltTaber.exe\"\r\n"
        "del \"%~f0\"\r\n"
    ).arg(QDir::toNativeSeparators(backupDir),
          QDir::toNativeSeparators(appDir),
          QDir::toNativeSeparators(markerPath));

    bat.write(content.toUtf8());
    bat.close();

    qWarning() << "[Rollback] Starting rollback from"
               << backupDir << "to" << appDir;

    if (!QProcess::startDetached(batPath)) {
        qWarning() << "[Rollback] Failed to start rollback script";
        return false;
    }

    QMetaObject::invokeMethod(qApp, "quit", Qt::QueuedConnection);
    return true;
}
