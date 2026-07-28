#ifndef WIN_SWITCHER_LOGGER_H
#define WIN_SWITCHER_LOGGER_H

#include <cstdint>
#include <QString>
#include <QDebug>
#include <QFile>
#include <QMutex>

namespace Util {

enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Fatal,
};

enum LogFlag : uint32_t {
    LogTrace = 1 << 0,
    LogDebug = 1 << 1,
    LogInfo  = 1 << 2,
    LogWarn  = 1 << 3,
    LogError = 1 << 4,
    LogFatal = 1 << 5,
};

using LogFlags = uint32_t;

constexpr LogFlags DEFAULT_LOG_FLAGS = LogWarn | LogError | LogFatal;

class Logger {
public:
    static void init();
    static void reconfigure();
    static void shutdown();
    static void closeLog();
    static void reopenLog();

    static void setActiveFlags(LogFlags flags);
    static LogFlags activeFlags();
    static void setLogDirectory(const QString& path);
    static QString logDirectory();

    static void trace(const char* file, int line, const QString& msg);

private:
    static void messageHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg);
    static void writeLog(LogLevel level, const char* file, int line, const QString& msg);
    static QString logFilePath();
    static void openLogFile();
    static void checkDateRollover();
    static bool shouldLog(LogLevel level);
    static QString levelString(LogLevel level);
    static LogFlag logLevelToFlag(LogLevel level);

    static QFile* m_file;
    static QMutex* m_mutex;
    static bool m_initialized;
    static LogFlags m_activeFlags;
    static QString m_logDir;
};

#define LOG_TRACE(msg) ::Util::Logger::trace(__FILE__, __LINE__, msg)

inline void checkSlowInit(const char* phaseName, qint64 elapsedMs, qint64 thresholdMs = 100) {
#ifndef NDEBUG
    if (elapsedMs > thresholdMs) {
        qWarning().noquote() << QStringLiteral("[SlowInit] %1 took %2 ms (threshold: %3 ms)")
            .arg(phaseName).arg(elapsedMs).arg(thresholdMs);
    }
#else
    Q_UNUSED(phaseName)
    Q_UNUSED(elapsedMs)
    Q_UNUSED(thresholdMs)
#endif
}

} // namespace Util

#endif //WIN_SWITCHER_LOGGER_H
