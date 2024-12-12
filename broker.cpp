// This is an open source non-commercial project. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include "broker.h"
#include <QtCore/QDebug>
#include <QtCore/QFile>

QT_USE_NAMESPACE
//==============================================
BrokerHelper::BrokerHelper(quint16 serverPort,
               quint16 clientPort,
               QObject *parent)
        :BrokerHelper(serverPort,
                clientPort,
                "",
                parent)
{}

BrokerHelper::BrokerHelper(quint16 serverPort,
               quint16 clientPort,
               QString server_password,
               QObject *parent)
    : QObject(parent)
    , server_password(server_password)
    , serverPort(serverPort)
    , clientPort(clientPort)
{
}

BrokerHelper::~BrokerHelper()
{
    this->stop();
}

bool BrokerHelper::start(void)
{
    qDebug() << Q_FUNC_INFO << QThread::currentThread();
    server_thread.start();
    server = new SslServer(serverPort, server_password);
    connect(&server_thread, &QThread::finished, server, [this](){ this->server->deleteLater(); });
    connect(server, &SslServer::started, this, &BrokerHelper::ssl_server_started, Qt::QueuedConnection);
    server->moveToThread(&server_thread);

    client_thread.start();
    client = new SslServer(clientPort);
    connect(&client_thread, &QThread::finished, client, [this](){ this->client->deleteLater(); });
    connect(client, &SslServer::started, this, &BrokerHelper::ssl_client_started, Qt::QueuedConnection);
    client->moveToThread(&client_thread);

    emit server->setServerName("Server:");
    emit server->start();
    emit client->setServerName("Client:");
    emit client->start();
    emit started();
    return true;
}

void BrokerHelper::ssl_server_started()
{
    SslServer *srv = qobject_cast<SslServer *>(sender());
    emit srv->setKeepaliveInterval(keepalive_interval);
    //Connect to log signals and chain them
    connect(srv, &SslServer::log, this, &BrokerHelper::log, Qt::QueuedConnection);
    //Connect to connect/disconnect signals
    connect(srv,     &SslServer::peerConnected,
            this, &BrokerHelper::server_connected, Qt::QueuedConnection);
    connect(srv,     &SslServer::peerDisconnected,
            this, &BrokerHelper::server_disconnected, Qt::QueuedConnection);
    //Connect to send/receive signals
    connect(srv,     &SslServer::sendTextMessageToBroker,
            this, &BrokerHelper::receiveTextMessageFromSslServer, Qt::DirectConnection);
    connect(srv,     &SslServer::sendBinaryMessageToBroker,
            this, &BrokerHelper::receiveBinaryMessageFromSslServer, Qt::DirectConnection);
    connect(this, &BrokerHelper::sendTextMessageToSslServer,
            srv,     &SslServer::receiveTextMessageFromBroker, Qt::DirectConnection);
    connect(this, &BrokerHelper::sendBinaryMessageToSslServer,
            srv,     &SslServer::receiveBinaryMessageFromBroker, Qt::DirectConnection);

}

void BrokerHelper::ssl_client_started()
{
    SslServer *clt = qobject_cast<SslServer *>(sender());
    emit clt->setKeepaliveInterval(keepalive_interval);
    //Connect to log signals and chain them
    connect(client, &SslServer::log, this, &BrokerHelper::log, Qt::QueuedConnection);
    //Connect to connect/disconnect signals
    connect(clt,     &SslServer::peerConnected,
            this, &BrokerHelper::client_connected, Qt::QueuedConnection);
    connect(clt,     &SslServer::peerDisconnected,
            this, &BrokerHelper::client_disconnected, Qt::QueuedConnection);
    //Connect to send/receive signals
    connect(clt,     &SslServer::sendTextMessageToBroker,
            this, &BrokerHelper::receiveTextMessageFromSslClient, Qt::DirectConnection);
    connect(clt,     &SslServer::sendBinaryMessageToBroker,
            this, &BrokerHelper::receiveBinaryMessageFromSslClient, Qt::DirectConnection);
    connect(this, &BrokerHelper::sendTextMessageToSslClient,
            clt,     &SslServer::receiveTextMessageFromBroker, Qt::DirectConnection);
    connect(this, &BrokerHelper::sendBinaryMessageToSslClient,
            clt,     &SslServer::receiveBinaryMessageFromBroker, Qt::DirectConnection);
}

void BrokerHelper::stop(void)
{
    qDebug() << "Broker: stopped";
    server_thread.exit();
    server_thread.wait();
    server = nullptr;

    client_thread.exit();
    client_thread.wait();
    client = nullptr;

    emit stopped();
}

//Returns true if message is good, false otherwise
//Must be applied to client only
//TODO Rename function - non-descriptive funcion name.
bool BrokerHelper::passClientTextMessage(QString &message)
{
    return true;
}

void BrokerHelper::receiveTextMessageFromSslServer(QString message)
{
    //qDebug() << "Broker: received text message from server";
    //qDebug() << "Broker: sending text message to client";
    emit sendTextMessageToSslClient(message);
}

void BrokerHelper::receiveTextMessageFromSslClient(QString message)
{
    //qDebug() << "Broker: received text message from client";
    if (passClientTextMessage(message))
    {
        //qDebug() << "Broker: sending text message to server";
        emit sendTextMessageToSslServer(message);
    }
    else
    {
        //qDebug() << "Broker: text message" << message << "filtered";
    }
}

void BrokerHelper::receiveBinaryMessageFromSslServer(QByteArray &message)
{
    //qDebug() << "Broker: received binary message from server";
    //qDebug() << "Broker: sending binary message to client";
    emit sendBinaryMessageToSslClient(message);
}

void BrokerHelper::receiveBinaryMessageFromSslClient(QByteArray &message)
{
    //qDebug() << "Broker: received binary message from client";
    //qDebug() << "Broker: sending binary message to server";
    emit sendBinaryMessageToSslServer(message);
}

void BrokerHelper::set_keepalive_interval(int ms)
{
    keepalive_interval = ms;
    if (server != nullptr)
        emit server->setKeepaliveInterval(ms);
    if (client != nullptr)
        emit client->setKeepaliveInterval(ms);
}

void BrokerHelper::enable_keepalive(bool enable)
{
    auto setup_keepalive = [](SslServer *s,
                              bool enable,
                              int keepalive_interval,
                              int keepalive_missed_limit)
    {
        emit s->setKeepaliveInterval(keepalive_interval);
        emit s->setKeepaliveMissedLimit(keepalive_missed_limit);
        emit s->enableKeepalive(enable);
    };

    keepalive_enabled = enable;
    if (server != nullptr)
    {
        setup_keepalive(server, enable, keepalive_interval, keepalive_missed_limit);
    }
    if (client != nullptr)
    {
        setup_keepalive(client, enable, keepalive_interval, keepalive_missed_limit);
    }
}

//TODO send signal to server and client
void BrokerHelper::set_keepalive_missed_limit(int limit)
{
    keepalive_missed_limit = limit;
}

//=================================================

Broker::Broker(quint16 serverPort,
                             quint16 clientPort,
                             QString server_password,
                             QObject *parent)
    : QObject(parent)
    , broker(new BrokerHelper(serverPort, clientPort, server_password, this))
{
    connect(broker, &BrokerHelper::log, this, &Broker::log, Qt::QueuedConnection);
    connect(broker, &BrokerHelper::client_connected, this, &Broker::client_connected, Qt::QueuedConnection);
    connect(broker, &BrokerHelper::client_disconnected, this, &Broker::client_disconnected, Qt::QueuedConnection);
    connect(broker, &BrokerHelper::server_connected, this, &Broker::server_connected, Qt::QueuedConnection);
    connect(broker, &BrokerHelper::server_disconnected, this, &Broker::server_disconnected, Qt::QueuedConnection);
    connect(broker, &BrokerHelper::started, this, &Broker::started, Qt::QueuedConnection);
    connect(broker, &BrokerHelper::stopped, this, &Broker::stopped, Qt::QueuedConnection);

    connect(this, &Broker::enableKeepalive, this, &Broker::enable_keepalive, Qt::QueuedConnection);
    connect(this, &Broker::setKeepaliveInterval, this, &Broker::set_keepalive_interval, Qt::QueuedConnection);
    connect(this, &Broker::setKeepaliveMissedLimit, this, &Broker::set_keepalive_missed_limit, Qt::QueuedConnection);
    connect(this, &Broker::start, this, &Broker::start_broker, Qt::QueuedConnection);
    connect(this, &Broker::stop, this, &Broker::stop_broker, Qt::DirectConnection);
}

Broker::~Broker()
{
    broker->stop();
}

void Broker::enable_keepalive(bool enable)
{
    broker->enable_keepalive(enable);
}

void Broker::start_broker()
{
    broker->start();
}

void Broker::stop_broker()
{
    broker->stop();
}

void Broker::set_keepalive_interval(int interval)
{
    broker->set_keepalive_interval(interval);
}

void Broker::set_keepalive_missed_limit(int limit)
{
    broker->set_keepalive_missed_limit(limit);
}
