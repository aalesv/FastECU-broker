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
    , serverPort(serverPort)
    , clientPort(clientPort)
    , server(new SslServer(serverPort, server_password))
    , client(new SslServer(clientPort))
{
    int k_int = 0;
    if (keepalive_enabled)
        k_int = keepalive_interval;
    emit server->setKeepaliveInterval(k_int);
    emit client->setKeepaliveInterval(k_int);
    //Connect to log signals and chain them
    connect(server, &SslServer::log, this, &BrokerHelper::log, Qt::QueuedConnection);
    connect(client, &SslServer::log, this, &BrokerHelper::log, Qt::QueuedConnection);
    //Connect to client and server connect/disconnect signals
    connect(server, &SslServer::peerConnected,
            this, &BrokerHelper::server_connected, Qt::QueuedConnection);
    connect(server, &SslServer::peerDisconnected,
            this, &BrokerHelper::server_disconnected, Qt::QueuedConnection);
    connect(client, &SslServer::peerConnected,
            this, &BrokerHelper::client_connected, Qt::QueuedConnection);
    connect(client, &SslServer::peerDisconnected,
            this, &BrokerHelper::client_disconnected, Qt::QueuedConnection);
    //Connect to send/receive signals
    connect(server, &SslServer::sendTextMessageToBroker,
            this, &BrokerHelper::receiveTextMessageFromSslServer, Qt::DirectConnection);
    connect(client, &SslServer::sendTextMessageToBroker,
            this, &BrokerHelper::receiveTextMessageFromSslClient, Qt::DirectConnection);

    connect(server, &SslServer::sendBinaryMessageToBroker,
            this, &BrokerHelper::receiveBinaryMessageFromSslServer, Qt::DirectConnection);
    connect(client, &SslServer::sendBinaryMessageToBroker,
            this, &BrokerHelper::receiveBinaryMessageFromSslClient, Qt::DirectConnection);

    connect(this, &BrokerHelper::sendTextMessageToSslServer,
            server, &SslServer::receiveTextMessageFromBroker, Qt::DirectConnection);
    connect(this, &BrokerHelper::sendTextMessageToSslClient,
            client, &SslServer::receiveTextMessageFromBroker, Qt::DirectConnection);

    connect(this, &BrokerHelper::sendBinaryMessageToSslServer,
            server, &SslServer::receiveBinaryMessageFromBroker, Qt::DirectConnection);
    connect(this, &BrokerHelper::sendBinaryMessageToSslClient,
            client, &SslServer::receiveBinaryMessageFromBroker, Qt::DirectConnection);
}

BrokerHelper::~BrokerHelper()
{
    this->stop();
}

bool BrokerHelper::start(void)
{
    emit server->setServerName("Server:");
    emit server->start();
    emit client->setServerName("Client:");
    emit client->start();
    emit started();
    return true;
}

void BrokerHelper::stop(void)
{
    qDebug() << "Broker: stopped";
    emit server->stop();
    emit client->stop();
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
        qDebug() << "Broker: text message" << message << "filtered";
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
    emit server->setKeepaliveInterval(ms);
    emit client->setKeepaliveInterval(ms);
}

void BrokerHelper::enable_keepalive(bool enable)
{
    keepalive_enabled = enable;
    /*if (enable)
    {
        qDebug() << "Enabling keepalives";
        server->set_keepalive_interval(keepalive_interval);
        client->set_keepalive_interval(keepalive_interval);
        server->start_keepalive();
        client->start_keepalive();
    }
    else
    {
        qDebug() << "Disabling keepalives";
        server->stop_keepalive();
        client->stop_keepalive();
        server->set_keepalive_interval(0);
        client->set_keepalive_interval(0);
    }*/
    emit server->setKeepaliveInterval(keepalive_interval);
    emit client->setKeepaliveInterval(keepalive_interval);
    emit server->enableKeepalive(enable);
    emit client->enableKeepalive(enable);
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
    //broker->deleteLater();
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
