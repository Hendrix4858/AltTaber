#include "utils/Logger.h"
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QDebug>
#include <QtGlobal>
#include <windows.h>

namespace Util {

QFile* Logger::m_file = nullptr;
QMutex* Logger::m_mutex = nullptr;
bool Logger::m_initialized = false;

static constexpr qint64 MaxLogSize = 1024 * 1024; // 1MB
static constexpr int MaxLogFiles = 3;

void Logger::init() {
    if (m_initialized) return;

    m_mutex = new QMutex;

    auto dir = logDir();
    QDir().mkpath(dir);

    auto filePath = dir + "/AltTaber.log";
    m_file = new QFile(filePath);
    if (m_file->open(QIODevice::WriteOnly)) {
        m_file->seek(m_file->size());
        if (m_file->size() == 0) {
            const char bom[] = "\xEF\xBB\xBF";
            m_file->write(bom, 3);
        }
    }

    m_initialized = true;
    qInstallMessageHandler(messageHandler);

    qInfo() << "Logger initialized, path:" << filePath;
}

void Logger::shutdown() {
    if (!m_initialized) return;

    qInfo() << "Logger shutting down";
    qInstallMessageHandler(nullptr);

    if (m_file) {
        m_file->flush();
        m_file->close();
        delete m_file;
        m_file = nullptr;
    }
    if (m_mutex) {
        delete m_mutex;
        m_mutex = nullptr;
    }
    m_initialized = false;
}

void Logger::messageHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg) {
#ifdef QT_NO_DEBUG
    if (type == QtDebugMsg) return;
#endif

    auto timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    auto level = levelString(type);
    auto file = ctx.file ? QFileInfo(ctx.file).fileName() : "unknown";
    auto line = ctx.line;

    auto logLine = QString("[%1] [%2] %3 (%4:%5)\n")
                       .arg(timestamp, level, msg, file)
                       .arg(line);

    OutputDebugStringW(logLine.toStdWString().c_str());

    if (m_file) {
        QMutexLocker lock(m_mutex);
        rotateIfNeeded();
        m_file->seek(m_file->size());
        auto utf8 = logLine.toUtf8();
        m_file->write(utf8.constData(), utf8.size());
        m_file->flush();
    }
}

void Logger::rotateIfNeeded() {
    if (!m_file) return;
    if (m_file->size() < MaxLogSize) return;

    m_file->flush();
    m_file->close();

    auto dir = logDir();
    for (int i = MaxLogFiles - 1; i > 0; --i) {
        auto oldName = QString("%1/AltTaber.log.%2").arg(dir).arg(i);
        auto newName = QString("%1/AltTaber.log.%2").arg(dir).arg(i + 1);
        if (QFile::exists(oldName)) {
            QFile::remove(newName);
            QFile::rename(oldName, newName);
        }
    }
    QFile::rename(dir + "/AltTaber.log", dir + "/AltTaber.log.1");

    m_file->open(QIODevice::WriteOnly);
    const char bom[] = "\xEF\xBB\xBF";
    m_file->write(bom, 3);
}

QString Logger::logDir() {
    return QApplication::applicationDirPath() + "/log";
}

QString Logger::levelString(QtMsgType type) {
    switch (type) {
        case QtDebugMsg:    return "DEBUG";
        case QtInfoMsg:     return "INFO";
        case QtWarningMsg:  return "WARN";
        case QtCriticalMsg: return "ERROR";
        case QtFatalMsg:    return "FATAL";
    }
    return "UNKNOWN";
}

} // namespace Util
