#ifndef BROKER_H
#define BROKER_H

#include <QtCore/QMetaType>
#include <QtCore/QObject>
#include <QtCore/QList>
#include <QtCore/QByteArray>
#include <QThread>
#include <QTimer>
#include "sslserver.h"

//Provides 2 websocket servers over SSL
//and message passing between two peers.
class BrokerHelper : public QObject
{
    Q_OBJECT
public:
    explicit BrokerHelper(quint16 serverPort,
                    quint16 clientPort,
                    QObject *parent = nullptr);

    explicit BrokerHelper(quint16 serverPort,
                    quint16 clientPort,
                    QString server_password,
                    QObject *parent = nullptr);
    ~BrokerHelper() override;

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
    bool started();
    bool stopped();

public slots:
    bool start(void);
    void stop(void);
    void receiveTextMessageFromSslServer(QString message);
    void receiveBinaryMessageFromSslServer(QByteArray &message);
    void receiveTextMessageFromSslClient(QString message);
    void receiveBinaryMessageFromSslClient(QByteArray &message);
    void enable_keepalive(bool enable);
    void set_keepalive_interval(int ms);
    void set_keepalive_missed_limit(int limit);
    void ssl_server_started();
    void ssl_client_started();

private:
    QThread server_thread;
    QThread client_thread;
    quint16 serverPort;
    quint16 clientPort;
    QString server_password = "";
    SslServer *server = nullptr;
    SslServer *client = nullptr;
    int keepalive_interval = 5000;
    int keepalive_missed_limit = 12;
    bool keepalive_enabled = false;
    bool passClientTextMessage(QString &message);
};

class Broker : public QObject
{
    Q_OBJECT
private:
    BrokerHelper *broker;
public:
    Broker(quint16 serverPort,
                  quint16 clientPort,
                  QString server_password,
                  QObject *parent = nullptr);
    ~Broker() override;

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

    void enableKeepalive(bool enable);
    void setKeepaliveInterval(int interval);
    void setKeepaliveMissedLimit(int limit);
    void start();
    void started();
    void stop();
    void stopped();

private slots:
    void enable_keepalive(bool enable);
    void set_keepalive_interval(int interval);
    void set_keepalive_missed_limit(int limit);
    void start_broker();
    void stop_broker();

};
#endif // BROKER_H
