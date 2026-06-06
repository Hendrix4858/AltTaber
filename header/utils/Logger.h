#ifndef WIN_SWITCHER_LOGGER_H
#define WIN_SWITCHER_LOGGER_H

#include <QString>
#include <QFile>
#include <QMutex>

namespace Util {

enum class LogLevel {
    All = 0,
    Debug,
    Info,
    Warning,
    Error,
    Fatal,
    None
};

class Logger {
public:
    static void init();
    static void reconfigure();
    static void shutdown();

    static void setLogLevel(LogLevel level);
    static LogLevel logLevel();
    static void setLogDirectory(const QString& path);
    static QString logDirectory();

private:
    static void messageHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg);
    static QString logFilePath();
    static void openLogFile();
    static void checkDateRollover();
    static bool shouldLog(QtMsgType type);
    static int logSeverity(QtMsgType type);
    static QString levelString(QtMsgType type);

    static QFile* m_file;
    static QMutex* m_mutex;
    static bool m_initialized;
    static LogLevel m_logLevel;
    static QString m_logDir;
};

} // namespace Util

#endif //WIN_SWITCHER_LOGGER_H
