// This is an open source non-commercial project. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include "broker.h"
#include <QtCore/QDebug>
#include <QtCore/QFile>

QT_USE_NAMESPACE
//==============================================
Broker::Broker(quint16 serverPort,
               quint16 clientPort,
               QObject *parent)
        :Broker(serverPort,
                clientPort,
                "",
                parent)
{}

Broker::Broker(quint16 serverPort,
               quint16 clientPort,
               QString server_password,
               QObject *parent)
    : QObject(parent)
    , serverPort(serverPort)
    , clientPort(clientPort)
    , server(SslServer(serverPort, server_password, this))
    , client(SslServer(clientPort, this))
{
    int k_int = 0;
    if (keepalive_enabled)
        k_int = keepalive_interval;
    server.set_keepalive_interval(k_int);
    client.set_keepalive_interval(k_int);
    //Connect to log signals and chain them
    connect(&server, &SslServer::log, this, &Broker::log, Qt::QueuedConnection);
    connect(&client, &SslServer::log, this, &Broker::log, Qt::QueuedConnection);
    //Connect to client and server connect/disconnect signals
    connect(&server, &SslServer::peerConnected,
            this, &Broker::server_connected, Qt::QueuedConnection);
    connect(&server, &SslServer::peerDisconnected,
            this, &Broker::server_disconnected, Qt::QueuedConnection);
    connect(&client, &SslServer::peerConnected,
            this, &Broker::client_connected, Qt::QueuedConnection);
    connect(&client, &SslServer::peerDisconnected,
            this, &Broker::client_disconnected, Qt::QueuedConnection);
    //Connect to send/receive signals
    connect(&server, &SslServer::sendTextMessageToBroker,
            this, &Broker::receiveTextMessageFromSslServer, Qt::DirectConnection);
    connect(&client, &SslServer::sendTextMessageToBroker,
            this, &Broker::receiveTextMessageFromSslClient, Qt::DirectConnection);

    connect(&server, &SslServer::sendBinaryMessageToBroker,
            this, &Broker::receiveBinaryMessageFromSslServer, Qt::DirectConnection);
    connect(&client, &SslServer::sendBinaryMessageToBroker,
            this, &Broker::receiveBinaryMessageFromSslClient, Qt::DirectConnection);

    connect(this, &Broker::sendTextMessageToSslServer,
            &server, &SslServer::receiveTextMessageFromBroker, Qt::DirectConnection);
    connect(this, &Broker::sendTextMessageToSslClient,
            &client, &SslServer::receiveTextMessageFromBroker, Qt::DirectConnection);

    connect(this, &Broker::sendBinaryMessageToSslServer,
            &server, &SslServer::receiveBinaryMessageFromBroker, Qt::DirectConnection);
    connect(this, &Broker::sendBinaryMessageToSslClient,
            &client, &SslServer::receiveBinaryMessageFromBroker, Qt::DirectConnection);
}

Broker::~Broker()
{
    this->stop();
}

bool Broker::start(void)
{
    bool r = server.start() && client.start();
    if (r)
    {
        qDebug() << "Broker: started";
        emit started();
    }
    else
    {
        qDebug() << "Failed to start broker";
        this->stop();
    }
    return r;
}

void Broker::stop(void)
{
    qDebug() << "Broker: stopped";
    server.stop();
    client.stop();
    emit stopped();
}

bool Broker::isSslCertFileFound()
{
    return server.isSslCertFileFound();
}

bool Broker::isSslKeyFileFound()
{
    return server.isSslKeyFileFound();
}

//Returns true if message is good, false otherwise
//Must be applied to client only
//TODO Rename function - non-descriptive funcion name.
bool Broker::passClientTextMessage(QString &message)
{
    return true;
}

void Broker::receiveTextMessageFromSslServer(QString message)
{
    //qDebug() << "Broker: received text message from server";
    //qDebug() << "Broker: sending text message to client";
    emit sendTextMessageToSslClient(message);
}

void Broker::receiveTextMessageFromSslClient(QString message)
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

void Broker::receiveBinaryMessageFromSslServer(QByteArray &message)
{
    //qDebug() << "Broker: received binary message from server";
    //qDebug() << "Broker: sending binary message to client";
    emit sendBinaryMessageToSslClient(message);
}

void Broker::receiveBinaryMessageFromSslClient(QByteArray &message)
{
    //qDebug() << "Broker: received binary message from client";
    //qDebug() << "Broker: sending binary message to server";
    emit sendBinaryMessageToSslServer(message);
}

void Broker::set_keepalive_interval(int ms)
{
    keepalive_interval = ms;
    server.set_keepalive_interval(ms);
    client.set_keepalive_interval(ms);
}

void Broker::enable_keepalive(bool enable)
{
    keepalive_enabled = enable;
    if (enable)
    {
        qDebug() << "Enabling keepalives";
        server.set_keepalive_interval(keepalive_interval);
        client.set_keepalive_interval(keepalive_interval);
        server.start_keepalive();
        client.start_keepalive();
    }
    else
    {
        qDebug() << "Disabling keepalives";
        server.stop_keepalive();
        client.stop_keepalive();
        server.set_keepalive_interval(0);
        client.set_keepalive_interval(0);
    }
}

BrokerWrapper::BrokerWrapper(quint16 serverPort,
                             quint16 clientPort,
                             QString server_password,
                             QObject *parent)
    : QObject(parent)
    , broker(new Broker(serverPort, clientPort, server_password, this))
{
    connect(broker, &Broker::log, this, &BrokerWrapper::log, Qt::QueuedConnection);
    connect(broker, &Broker::client_connected, this, &BrokerWrapper::client_connected, Qt::QueuedConnection);
    connect(broker, &Broker::client_disconnected, this, &BrokerWrapper::client_disconnected, Qt::QueuedConnection);
    connect(broker, &Broker::server_connected, this, &BrokerWrapper::server_connected, Qt::QueuedConnection);
    connect(broker, &Broker::server_disconnected, this, &BrokerWrapper::server_disconnected, Qt::QueuedConnection);
    connect(broker, &Broker::started, this, &BrokerWrapper::started, Qt::QueuedConnection);
    connect(broker, &Broker::stopped, this, &BrokerWrapper::stopped, Qt::QueuedConnection);

    connect(this, &BrokerWrapper::enable_keepalive, this, &BrokerWrapper::set_keepalive);
    connect(this, &BrokerWrapper::start, this, &BrokerWrapper::start_broker);
    connect(this, &BrokerWrapper::stop, this, &BrokerWrapper::stop_broker, Qt::DirectConnection);
}

BrokerWrapper::~BrokerWrapper()
{
    broker->stop();
    //broker->deleteLater();
}

bool BrokerWrapper::isSslCertFileFound()
{
    return broker->isSslCertFileFound();
}

bool BrokerWrapper::isSslKeyFileFound()
{
    return broker->isSslKeyFileFound();
}

void BrokerWrapper::set_keepalive(bool enable)
{
    broker->enable_keepalive(enable);
}

void BrokerWrapper::start_broker()
{
    if (!broker->isSslCertFileFound())
        emit log("Failed to open localhost.cert file");
    if (!broker->isSslKeyFileFound())
        emit log("Failed to open localhost.key file");
    broker->start();
}

void BrokerWrapper::stop_broker()
{
    broker->stop();
}
