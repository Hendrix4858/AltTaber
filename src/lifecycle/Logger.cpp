#include "lifecycle/Logger.h"
#include "core/ConfigManager.h"
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
LogFlags Logger::m_activeFlags = DEFAULT_LOG_FLAGS;
QString Logger::m_logDir;

void Logger::init() {
    if (m_initialized) return;

    m_mutex = new QMutex;

    m_activeFlags = cfg().getLogFlags();
    m_logDir = cfg().getLogDirectory();

    openLogFile();

    m_initialized = true;
    qInstallMessageHandler(messageHandler);

    qInfo() << "Logger initialized, flags:" << static_cast<int>(m_activeFlags)
            << "path:" << (m_file ? m_file->fileName() : "none");
}

void Logger::reconfigure() {
    if (!m_initialized) return;

    auto newFlags = cfg().getLogFlags();
    auto newDir = cfg().getLogDirectory();

    {
        QMutexLocker lock(m_mutex);

        bool flagsChanged = (newFlags != m_activeFlags);
        bool dirChanged = (newDir != m_logDir);

        if (dirChanged) {
            m_logDir = newDir;
            if (m_file) {
                m_file->flush();
                m_file->close();
                delete m_file;
                m_file = nullptr;
            }
            openLogFile();
        }

        m_activeFlags = newFlags;
    }

    qInfo() << "Logger reconfigured, flags:" << static_cast<int>(m_activeFlags)
            << "path:" << (m_file ? m_file->fileName() : "none");
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

void Logger::setActiveFlags(LogFlags flags) {
    m_activeFlags = flags;
}

LogFlags Logger::activeFlags() {
    return m_activeFlags;
}

void Logger::setLogDirectory(const QString& path) {
    m_logDir = path;
}

QString Logger::logDirectory() {
    return m_logDir;
}

QString Logger::logFilePath() {
    auto dir = m_logDir.isEmpty()
        ? QApplication::applicationDirPath() + "/log"
        : m_logDir;
    auto now = QDateTime::currentDateTime().toString("yyyy-MM-dd");
    return dir + "/" + now + ".log";
}

void Logger::openLogFile() {
    auto dir = m_logDir.isEmpty()
        ? QApplication::applicationDirPath() + "/log"
        : m_logDir;
    QDir().mkpath(dir);

    auto filePath = logFilePath();
    m_file = new QFile(filePath);
    if (m_file->open(QIODevice::Append | QIODevice::WriteOnly)) {
        if (m_file->size() == 0) {
            const char bom[] = "\xEF\xBB\xBF";
            m_file->write(bom, 3);
        }
    }
}

void Logger::checkDateRollover() {
    if (!m_file) return;
    auto expectedPath = logFilePath();
    if (m_file->fileName() != expectedPath) {
        m_file->flush();
        m_file->close();
        delete m_file;
        m_file = nullptr;
        openLogFile();
    }
}

bool Logger::shouldLog(QtMsgType type) {
    LogFlag flag;
    switch (type) {
        case QtDebugMsg:    flag = LogDebug; break;
        case QtInfoMsg:     flag = LogInfo; break;
        case QtWarningMsg:  flag = LogWarn; break;
        case QtCriticalMsg: flag = LogError; break;
        case QtFatalMsg:    flag = LogFatal; break;
        default:            return false;
    }
    return (m_activeFlags & flag) != 0;
}

void Logger::messageHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg) {
    if (!shouldLog(type)) return;

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
        checkDateRollover();
        m_file->seek(m_file->size());
        auto utf8 = logLine.toUtf8();
        m_file->write(utf8.constData(), utf8.size());
        m_file->flush();
    }
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
