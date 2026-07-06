#include "lifecycle/IpcServer.h"
#include <QCoreApplication>
#include <QTimer>
#include <QDebug>

IpcServer::IpcServer(QObject* parent)
    : QObject(parent)
    , m_server(new QLocalServer(this))
{
}

IpcServer::~IpcServer()
{
    if (m_server->isListening()) {
        qInfo() << "[IPC] Server closed";
        m_server->close();
    }
}

bool IpcServer::start()
{
    m_server->setSocketOptions(QLocalServer::WorldAccessOption);

    auto tryListen = [this]() -> bool {
        return m_server->listen("AltTaber-MrBeanCpp-IPC");
    };

    if (!tryListen()) {
        if (m_server->serverError() == QAbstractSocket::AddressInUseError) {
            qWarning() << "[IPC] Address in use, removing stale server";
            QLocalServer::removeServer("AltTaber-MrBeanCpp-IPC");
            if (!tryListen()) {
                qWarning() << "[IPC] Retry failed after stale cleanup:" << m_server->errorString();
                return false;
            }
            qInfo() << "[IPC] Stale server removed, listening";
        } else {
            qWarning() << "[IPC] Failed to start server:" << m_server->errorString();
            return false;
        }
    }

    connect(m_server, &QLocalServer::newConnection,
            this, &IpcServer::onNewConnection);
    qInfo() << "[IPC] Server listening on" << serverName();
    return true;
}

QString IpcServer::serverName() const
{
    return m_server->fullServerName();
}

void IpcServer::onNewConnection()
{
    auto* socket = m_server->nextPendingConnection();
    if (!socket) {
        qWarning() << "[IPC] newConnection signal but no socket available";
        return;
    }

    qInfo() << "[IPC] Client connected";

    connect(socket, &QLocalSocket::readyRead,
            this, &IpcServer::onReadyRead);
    connect(socket, &QLocalSocket::disconnected,
            socket, &QLocalSocket::deleteLater);
    connect(socket, &QLocalSocket::disconnected, this, [socket]() {
        qInfo() << "[IPC] Client disconnected";
    });
}

void IpcServer::onReadyRead()
{
    auto* socket = qobject_cast<QLocalSocket*>(sender());
    if (!socket) {
        qWarning() << "[IPC] readyRead from unknown sender";
        return;
    }

    QByteArray cmd = socket->readAll().trimmed();
    qInfo() << "[IPC] Command:" << cmd;
    handleCommand(socket, cmd);
}

void IpcServer::handleCommand(QLocalSocket* socket, const QByteArray& cmd)
{
    if (cmd == "quit") {
        qInfo() << "[IPC] quit: sending OK, scheduling app quit in 50ms";
        socket->write("OK\n");
        socket->flush();
        emit quitRequested();
    } else if (cmd == "ping") {
        socket->write("OK\n");
        socket->flush();
    } else {
        qWarning() << "[IPC] Unknown command:" << cmd;
        socket->write("ERR\n");
        socket->flush();
    }
}
