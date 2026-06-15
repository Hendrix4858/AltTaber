#ifndef WIN_SWITCHER_LOGGER_H
#define WIN_SWITCHER_LOGGER_H

#include <QString>
#include <QFile>
#include <QMutex>
#include <cstdint>

namespace Util {

enum LogFlag : uint32_t {
    LogDebug = 1 << 0,
    LogInfo  = 1 << 1,
    LogWarn  = 1 << 2,
    LogError = 1 << 3,
    LogFatal = 1 << 4,
};

using LogFlags = uint32_t;

constexpr LogFlags DEFAULT_LOG_FLAGS = LogWarn | LogError | LogFatal;

class Logger {
public:
    static void init();
    static void reconfigure();
    static void shutdown();

    static void setActiveFlags(LogFlags flags);
    static LogFlags activeFlags();
    static void setLogDirectory(const QString& path);
    static QString logDirectory();

private:
    static void messageHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg);
    static QString logFilePath();
    static void openLogFile();
    static void checkDateRollover();
    static bool shouldLog(QtMsgType type);
    static QString levelString(QtMsgType type);

    static QFile* m_file;
    static QMutex* m_mutex;
    static bool m_initialized;
    static LogFlags m_activeFlags;
    static QString m_logDir;
};

} // namespace Util

#endif //WIN_SWITCHER_LOGGER_H
