#include <QCoreApplication>
#include <QLocalSocket>
#include <QTimer>
#include <cstdio>

static constexpr auto ServerName = "AltTaber-MrBeanCpp-IPC";
static constexpr int ConnectTimeoutMs = 3000;
static constexpr int ResponseTimeoutMs = 3000;
static constexpr int QuitWaitMs = 10000;

enum ExitCode : int {
    Success = 0,
    BadArgs = 1,
    NotRunning = 2,
    Failure = 3,
};

static int cmdPing(QLocalSocket& socket) {
    socket.connectToServer(ServerName);
    if (!socket.waitForConnected(ConnectTimeoutMs))
        return NotRunning;

    socket.write("ping\n");
    if (!socket.waitForBytesWritten(ResponseTimeoutMs)) return Failure;
    if (!socket.waitForReadyRead(ResponseTimeoutMs)) return Failure;

    auto resp = QString::fromUtf8(socket.readAll()).trimmed();
    return (resp == "OK") ? Success : Failure;
}

static int cmdQuit(QLocalSocket& socket) {
    socket.connectToServer(ServerName);
    if (!socket.waitForConnected(ConnectTimeoutMs))
        return NotRunning;

    socket.write("quit\n");
    if (!socket.waitForBytesWritten(ResponseTimeoutMs)) return Failure;
    if (!socket.waitForReadyRead(ResponseTimeoutMs)) return Failure;

    auto resp = QString::fromUtf8(socket.readAll()).trimmed();
    if (resp != "OK") return Failure;

    socket.waitForDisconnected(QuitWaitMs);
    return Success;
}

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    auto args = app.arguments();
    if (args.size() < 2) {
        std::fputs("Usage: AltTaberCtl <ping|quit>\n", stderr);
        return BadArgs;
    }

    QLocalSocket socket;
    auto cmd = args[1];

    int code;
    if (cmd == "ping")
        code = cmdPing(socket);
    else if (cmd == "quit")
        code = cmdQuit(socket);
    else {
        std::fprintf(stderr, "Unknown command: %s\n", qPrintable(cmd));
        return BadArgs;
    }

    return code;
}
