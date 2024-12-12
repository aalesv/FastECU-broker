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

private:
    quint16 serverPort;
    quint16 clientPort;
    QString server_password = "";
    SslServer server;
    SslServer client;
    int keepalive_interval = 5000;
    bool keepalive_enabled = false;
    bool passClientTextMessage(QString &message);
};

class BrokerWrapper : public QObject
{
    Q_OBJECT
private:
    Broker *broker;
public:
    BrokerWrapper(quint16 serverPort,
                  quint16 clientPort,
                  QString server_password,
                  QObject *parent = nullptr);
    ~BrokerWrapper() override;

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

    void enable_keepalive(bool enable);
    void start();
    void started();
    void stop();
    void stopped();

private slots:
    void set_keepalive(bool enable);
    void start_broker();
    void stop_broker();

};
#endif // BROKER_H
