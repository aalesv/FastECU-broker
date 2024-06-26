// This is an open source non-commercial project. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include "broker.h"
#include "QtWebSockets/QWebSocketServer"
#include "QtWebSockets/QWebSocket"
#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtNetwork/QSslCertificate>
#include <QtNetwork/QSslKey>

QT_USE_NAMESPACE

QString QHostAddressToString(QHostAddress address)
{
    bool conversionOK = false;
    QHostAddress ip4Address(address.toIPv4Address(&conversionOK));
    QString ip4String;
    if (conversionOK)
    {
        ip4String = ip4Address.toString();
    }
    return ip4String;
}

SslServer::SslServer(quint16 port, QObject *parent)
    : SslServer(port, "", parent)
{}

SslServer::SslServer(quint16 port, QString allowedPath, QObject *parent)
    : QObject(parent)
    , port(port)
    , pWebSocketServer(nullptr)
    , allowedPath(allowedPath)
{
    pWebSocketServer = new QWebSocketServer(QStringLiteral("FastECU Broker Server"),
                                            QWebSocketServer::SecureMode,
                                            this);
    QSslConfiguration sslConfiguration;
    QFile certFile(QStringLiteral("localhost.cert"));
    QFile keyFile(QStringLiteral("localhost.key"));
    certFile.open(QIODevice::ReadOnly);
    keyFile.open(QIODevice::ReadOnly);
    QSslCertificate certificate(&certFile, QSsl::Pem);
    QSslKey sslKey(&keyFile, QSsl::Rsa, QSsl::Pem);
    certFile.close();
    keyFile.close();
    sslConfiguration.setPeerVerifyMode(QSslSocket::VerifyNone);
    sslConfiguration.setLocalCertificate(certificate);
    sslConfiguration.setPrivateKey(sslKey);
    pWebSocketServer->setSslConfiguration(sslConfiguration);
    pWebSocketServer->setMaxPendingConnections(1);
}

SslServer::~SslServer()
{
    this->stop();
    pWebSocketServer->deleteLater();
}

bool SslServer::start(void)
{
    //Start a server
    bool r = pWebSocketServer->listen(QHostAddress::Any, port);
    if (r)
    {
        qDebug() << "SslServer: server listening on port" << port;
        connect(pWebSocketServer, &QWebSocketServer::newConnection,
                this, &SslServer::onNewConnection, Qt::DirectConnection);
        connect(pWebSocketServer, &QWebSocketServer::sslErrors,
                this, &SslServer::onSslErrors,Qt::DirectConnection);
    }
    else
    {
        qDebug() << "SslServer: failed to start server on port" << port;
        emit log("Failed to open port "+QString::number(port));
    }
    return r;
}

void SslServer::stop(void)
{
    qDebug() << "SslServer: server stopping on port" << pWebSocketServer->serverPort();
    pWebSocketServer->close();
}

void SslServer::onNewConnection()
{
    QWebSocket *pSocket = pWebSocketServer->nextPendingConnection();

    if (pSocket == nullptr)
    {
        return;
    }

    //We can serve only one client
    if (peer != nullptr)
    {
        delete pSocket;
        return;
    }

    QString p_addr = QHostAddressToString(pSocket->peerAddress());
    QString p_port = QString::number(pSocket->peerPort());

    //Allow connections only at certain path
    if (pSocket->requestUrl().path() != "/"+allowedPath)
    {
        emit log("SslServer: Peer "+p_addr+":"+p_port+" tries to connect at invalid path");
        qDebug() << "SslServer: Peer " << p_addr << p_port << "tries to connect at invalid path";
        delete pSocket;
        return;
    }

    emit log("SslServer: Peer connected: "+p_addr+":"+p_port);
    qDebug() << "SslServer: Peer connected:" << p_addr << p_port;
    //Qt::DirectConnection mode calls slot immediately after signal emittig
    connect(pSocket, &QWebSocket::textMessageReceived,
            this, &SslServer::processTextMessage, Qt::DirectConnection);
    connect(pSocket, &QWebSocket::binaryMessageReceived,
            this, &SslServer::processBinaryMessage, Qt::DirectConnection);
    connect(pSocket, &QWebSocket::disconnected,
            this, &SslServer::socketDisconnected, Qt::DirectConnection);

    peer = pSocket;
    emit peerConnected("");
}

//Message is received from network, send it to broker
void SslServer::processTextMessage(QString message)
{
    qDebug() << "SslServer: Peer sent text message";
    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    if (pClient !=nullptr)
    {
        qDebug() << "SslServer: Sending message to broker";
        emit sendTextMessageToBroker(message);
    }
}

//Message is received from network, send it to broker
void SslServer::processBinaryMessage(QByteArray message)
{
    qDebug() << "SslServer: Peer sent binary message";
    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    if (pClient !=nullptr)
    {
        qDebug() << "SslServer: Sending message to broker";
        emit sendBinaryMessageToBroker(message);
    }
}

//Message is receiver from broker, send it to network
void SslServer::receiveTextMessageFromBroker(QString message)
{
    qDebug() << "SslServer: Received text message from broker";
    if (peer != nullptr)
    {
        qDebug() << "SslServer: Sending text message to peer";
        peer->sendTextMessage(message);
    }
}

//Message is receiver from broker, send it to network
void SslServer::receiveBinaryMessageFromBroker(QByteArray &message)
{
    qDebug() << "SslServer: Received binary message from broker";
    if (peer != nullptr)
    {
        qDebug() << "SslServer: Sending binary message to peer";
        peer->sendBinaryMessage(message);
    }
}

void SslServer::socketDisconnected()
{
    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    if (pClient)
    {
        QString p_addr = QHostAddressToString(pClient->peerAddress());
        QString p_port = QString::number(pClient->peerPort());
        emit log("Peer disconnected: "+p_addr+":"+p_port);
        qDebug() << "SslServer: Peer disconnected:" << p_addr << p_port;

        peer = nullptr;
        pClient->deleteLater();
        emit peerDisconnected("");
    }
}

void SslServer::onSslErrors(const QList<QSslError> &)
{
    emit log("SSL server errors occurred");
    qDebug() << "SslServer: Ssl errors occurred";
}

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
    qDebug() << "Broker: received text message from server";
    qDebug() << "Broker: sending text message to client";
    emit sendTextMessageToSslClient(message);
}

void Broker::receiveTextMessageFromSslClient(QString message)
{
    qDebug() << "Broker: received text message from client";
    if (passClientTextMessage(message))
    {
        qDebug() << "Broker: sending text message to server";
        emit sendTextMessageToSslServer(message);
    }
    else
    {
        qDebug() << "Broker: message filtered";
    }
}

void Broker::receiveBinaryMessageFromSslServer(QByteArray &message)
{
    qDebug() << "Broker: received binary message from server";
    qDebug() << "Broker: sending binary message to client";
    emit sendBinaryMessageToSslClient(message);
}

void Broker::receiveBinaryMessageFromSslClient(QByteArray &message)
{
    qDebug() << "Broker: received binary message from client";
    qDebug() << "Broker: sending binary message to server";
    emit sendBinaryMessageToSslServer(message);
}
