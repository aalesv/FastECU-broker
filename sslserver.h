#ifndef SSLSERVER_H
#define SSLSERVER_H

#include <QtCore/QMetaType>
#include <QtCore/QObject>
#include <QtCore/QList>
#include <QtCore/QByteArray>
#include <QtNetwork/QSslError>
#include <QThread>
#include <QTimer>
#include "QtWebSockets/QWebSocketServer"
#include "QtWebSockets/QWebSocket"
#include <QtNetwork/QSslCertificate>
#include <QtNetwork/QSslKey>

//Provides 1 connection on a specified port
//Websocket over SSL
class SslServerHelper : public QObject
{
    Q_OBJECT
public:
    explicit SslServerHelper(quint16 port, QObject *parent = nullptr);
    explicit SslServerHelper(quint16 port, QString allowedPath = "", QObject *parent = nullptr);
    ~SslServerHelper() override;

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
    void set_keepalive_interval(int ms) { keepalive_interval = ms; }
    void set_keepalive_missed_limit(int limit) { pings_sequently_missed_limit = limit; }
    void set_server_name(QString name) { server_name = name; }
    void enable_keepalive(bool enable);
    void start_keepalive();
    void stop_keepalive();

signals:
    void enableKeepailve(bool enable);
    void log(QString message);
    void sendTextMessageToBroker(QString message);
    void sendBinaryMessageToBroker(QByteArray &message);
    void peerConnected(QString message);
    void peerDisconnected(QString message);
    void started();
    void stopped();

private:
    bool certFileFound = false;
    bool keyFileFound = false;

    quint16 port;
    QWebSocketServer *pWebSocketServer;
    QWebSocket * peer = nullptr;
    //Allow to connect only at '/allowedPath' URL
    QString allowedPath = "";
    QString server_name = "SslServer:";

    bool keepalive_enabled = false;
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

class SslServer : public QObject
{
    Q_OBJECT
private:
    SslServerHelper *server;
    void enable_keepalive(bool enable);
    bool keepalive_enabled = false;

public:
    explicit SslServer(quint16 port, QString allowedPath = "", QObject *parent = nullptr);
    ~SslServer() override;

signals:
    void enableKeepalive(bool enable);
    void log(QString message);
    void peerConnected(QString message);
    void peerDisconnected(QString message);
    void receiveBinaryMessageFromBroker(QByteArray &message);
    void receiveTextMessageFromBroker(QString message);
    void sendBinaryMessageToBroker(QByteArray &message);
    void sendTextMessageToBroker(QString message);
    void setServerName(QString name);
    void setKeepaliveInterval(int interval);
    void setKeepaliveMissedLimit(int limit);
    void start(void);
    void started(void);
    void stop(void);
    void stopped(void);
};
#endif // SSLSERVER_H
