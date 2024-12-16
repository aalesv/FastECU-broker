#ifndef BROKER_H
#define BROKER_H

#include <QtCore/QMetaType>
#include <QtCore/QObject>
#include <QtCore/QList>
#include <QtCore/QByteArray>
#include <QtNetwork/QSslError>
#include <QTimer>
#include <chrono>

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

    bool isPeerConnected();
    bool isSslCertFileFound();
    bool isSslKeyFileFound();

public slots:
    void receiveTextMessageFromBroker(QString message);
    void receiveBinaryMessageFromBroker(QByteArray &message);
    void setName(QString name) { serverName = name; }
    void set_keepalive_interval(int ms) { keepalive_interval = ms; }
    void set_keepalive_missed_limit(int limit){ pings_sequently_missed_limit = limit; }
    bool start(void);
    void stop(void);
    void start_keepalive();
    void stop_keepalive();

signals:
    void connectionHung();
    void connectionRestored();
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
    bool is_keepalive_active() { return keepalive_timer->isActive(); }

    std::chrono::time_point<std::chrono::high_resolution_clock> last_input_packet_time;
    //When no incoming packets have arrived from a network for a certain period of time,
    //consider the connection hung
    int hanged_connection_interval = 120*1000;
    void check_connection(void);
    void connection_restored(void);
    bool hanged_connection_flag = false;

private slots:
    void onNewConnection();
    void onSslErrors(const QList<QSslError> &errors);
    void pong(quint64 elapsedTime, const QByteArray &payload);
    void processBinaryMessage(QByteArray message);
    void processTextMessage(QString message);
    void send_keepalive();
    void socketDisconnected();
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
    void enable_keepalive(bool enable);

private slots:
    void receiveTextMessageFromSslServer(QString message);
    void receiveBinaryMessageFromSslServer(QByteArray &message);
    void receiveTextMessageFromSslClient(QString message);
    void receiveBinaryMessageFromSslClient(QByteArray &message);
    void set_keepalive_interval(int ms);
    void server_connected(QString message);
    void server_disconnected(QString message);
    void client_connected(QString message);
    void client_disconnected(QString message);
    void connection_hung();
    void connection_restored();

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
