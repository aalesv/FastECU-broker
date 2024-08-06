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
    explicit SslServer(quint16 port, QString allowedPath = "", QObject *parent = nullptr);
    ~SslServer() override;

    bool isSslCertFileFound();
    bool isSslKeyFileFound();

private slots:
    void onNewConnection();
    void processTextMessage(QString message);
    void processBinaryMessage(QByteArray message);
    void socketDisconnected();
    void onSslErrors(const QList<QSslError> &errors);

public slots:
    bool start(void);
    void stop(void);
    void receiveTextMessageFromBroker(QString message);
    void receiveBinaryMessageFromBroker(QByteArray &message);

signals:
    void log(QString message);
    void sendTextMessageToBroker(QString message);
    void sendBinaryMessageToBroker(QByteArray &message);
    void peerConnected(QString message);
    void peerDisconnected(QString message);

private:
    bool certFileFound = false;
    bool keyFileFound = false;

    quint16 port;
    QWebSocketServer *pWebSocketServer;
    QWebSocket * peer = nullptr;
    //Allow to connect only at '/allowedPath' URL
    QString allowedPath = "";
};

class Broker : public QObject
{
    Q_OBJECT
public:
    explicit Broker(quint16 serverPort,
                    quint16 clientPort,
                    QObject *parent = nullptr);

    explicit Broker(quint16 serverPort,
                    quint16 clientPort,
                    QString server_password,
                    QObject *parent = nullptr);
    ~Broker() override;

    bool isSslCertFileFound();
    bool isSslKeyFileFound();

signals:
    void log(QString message);
    void sendTextMessageToSslServer(QString message);
    void sendBinaryMessageToSslServer(QByteArray &message);
    void sendTextMessageToSslClient(QString message);
    void sendBinaryMessageToSslClient(QByteArray &message);
    void server_connected(QString message);
    void server_disconnected(QString message);
    void client_connected(QString message);
    void client_disconnected(QString message);

public slots:
    bool start(void);
    void stop(void);
    void receiveTextMessageFromSslServer(QString message);
    void receiveBinaryMessageFromSslServer(QByteArray &message);
    void receiveTextMessageFromSslClient(QString message);
    void receiveBinaryMessageFromSslClient(QByteArray &message);

private:
    quint16 serverPort;
    quint16 clientPort;
    QString server_password = "";
    SslServer server;
    SslServer client;
    bool passClientTextMessage(QString &message);
};
#endif // BROKER_H
