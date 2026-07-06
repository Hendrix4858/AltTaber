#ifndef WIN_SWITCHER_IPCSERVER_H
#define WIN_SWITCHER_IPCSERVER_H

#include <QObject>
#include <QLocalServer>
#include <QLocalSocket>

class IpcServer : public QObject {
    Q_OBJECT
public:
    explicit IpcServer(QObject* parent = nullptr);
    ~IpcServer() override;

    bool start();
    QString serverName() const;

signals:
    void quitRequested();

private slots:
    void onNewConnection();
    void onReadyRead();

private:
    void handleCommand(QLocalSocket* socket, const QByteArray& cmd);

    QLocalServer* m_server = nullptr;
};

#endif // WIN_SWITCHER_IPCSERVER_H
