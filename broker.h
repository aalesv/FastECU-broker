#ifndef BROKER_H
#define BROKER_H

#include <QtCore/QMetaType>
#include <QtCore/QObject>
#include <QtCore/QList>
#include <QtCore/QByteArray>
#include <QtNetwork/QSslError>
#include <QTimer>

QT_FORWARD_DECLARE_CLASS(QWebSocketServer)
QT_FORWARD_DECLARE_CLASS(QWebSocket)

//Provides 1 connection on a specified port
//Websocket over SSL
class SslServer : public QObject
{
    Q_OBJECT
public:
    explicit SslServer(quint16 port, QObject *parent = nullptr);
    explicit SslServer(quint16 port, QString allowedPath = "", QObject *parent = nullptr);
    ~SslServer() override;

    bool isSslCertFileFound();
    bool isSslKeyFileFound();
    bool isPeerConnected();

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
    void set_keepalive_interval(int ms) { keepalive_interval = ms; }
    void set_keepalive_missed_limit(int limit){ pings_sequently_missed_limit = limit; }
    void start_keepalive();
    void stop_keepalive();
    void setName(QString name) { serverName = name; }

signals:
    void log(QString message);
    void sendTextMessageToBroker(QString message);
    void sendBinaryMessageToBroker(QByteArray &message);
    void peerConnected(QString message);
    void peerDisconnected(QString message);

private:
    QString serverName = "SslServer:";
    bool certFileFound = false;
    bool keyFileFound = false;

    quint16 port;
    QWebSocketServer *pWebSocketServer;
    QWebSocket * peer = nullptr;
    //Allow to connect only at '/allowedPath' URL
    QString allowedPath = "";

    int keepalive_interval = 0;
    QTimer *keepalive_timer;
    int keepalive_payload_pos = 0;
    int pings_sequently_missed = 0;
    int pings_sequently_missed_limit = 12;
    void ping(const QByteArray &payload = QByteArray());

private slots:
    void send_keepalive();
    void pong(quint64 elapsedTime, const QByteArray &payload);
};

//Provides 2 websocket servers over SSL
//and message passing between two peers.
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
    void serverConnected(QString message);
    void serverDisconnected(QString message);
    void clientConnected(QString message);
    void clientDisconnected(QString message);

public slots:
    bool start(void);
    void stop(void);
    void receiveTextMessageFromSslServer(QString message);
    void receiveBinaryMessageFromSslServer(QByteArray &message);
    void receiveTextMessageFromSslClient(QString message);
    void receiveBinaryMessageFromSslClient(QByteArray &message);
    void enable_keepalive(bool enable);
    void set_keepalive_interval(int ms);
    void server_connected(QString message);
    void server_disconnected(QString message);
    void client_connected(QString message);
    void client_disconnected(QString message);

private:
    quint16 serverPort;
    quint16 clientPort;
    QString server_password = "";
    SslServer server;
    SslServer client;
    int keepalive_interval = 5000;
    int keepalive_missed_limit = 12;
    bool keepalive_enabled = false;
    bool passClientTextMessage(QString &message);
};
#endif // BROKER_H
