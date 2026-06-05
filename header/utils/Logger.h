#ifndef WIN_SWITCHER_LOGGER_H
#define WIN_SWITCHER_LOGGER_H

#include <QString>
#include <QFile>
#include <QMutex>

namespace Util {

class Logger {
public:
    static void init();
    static void shutdown();

private:
    static void messageHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg);
    static QString logDir();
    static void rotateIfNeeded();
    static QString levelString(QtMsgType type);

    static QFile* m_file;
    static QMutex* m_mutex;
    static bool m_initialized;
};

} // namespace Util

#endif //WIN_SWITCHER_LOGGER_H
