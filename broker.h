#ifndef BROKER_H
#define BROKER_H

#include <QtCore/QMetaType>
#include <QtCore/QObject>
#include <QtCore/QList>
#include <QtCore/QByteArray>
#include <QtNetwork/QSslError>

QT_FORWARD_DECLARE_CLASS(QWebSocketServer)
QT_FORWARD_DECLARE_CLASS(QWebSocket)

//Provide 1 connection on a specified port
//Websocket ovet SSL
//With callback when data is received
class SslServer : public QObject
{
    Q_OBJECT
public:
    explicit SslServer(quint16 port, QObject *parent = nullptr);
    ~SslServer() override;

private slots:
    void onNewConnection();
    void processTextMessage(QString message);
    void processBinaryMessage(QByteArray message);
    void socketDisconnected();
    void onSslErrors(const QList<QSslError> &errors);

public slots:
    void receiveTextMessageFromBroker(QString message);
    void receiveBinaryMessageFromBroker(QByteArray message);

signals:
    void log(QString message);
    void sendTextMessageToBroker(QString message);
    void peerConnected(QString message);
    void peerDisconnected(QString message);

private:
    QWebSocketServer *pWebSocketServer;
    QWebSocket * peer = nullptr;
};

class Broker : public QObject
{
    Q_OBJECT
public:
    explicit Broker(quint16 serverPort,
                    quint16 clientPort,
                    QObject *parent = nullptr);
    ~Broker() override;

signals:
    void log(QString message);
    void sendTextMessageToSslServer(QString message);
    void sendTextMessageToSslClient(QString message);
    void server_connected(QString message);
    void server_disconnected(QString message);
    void client_connected(QString message);
    void client_disconnected(QString message);

public slots:
    void receiveTextMessageFromSslServer(QString message);
    void receiveTextMessageFromSslClient(QString message);

private:
    quint16 serverPort;
    quint16 clientPort;
    SslServer server;
    SslServer client;
    bool passClientTextMessage(QString &message);
};
#endif // BROKER_H
