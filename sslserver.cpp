// This is an open source non-commercial project. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "sslserver.h"
#include <QtCore/QFile>

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

SslServerHelper::SslServerHelper(quint16 port, QObject *parent)
    : SslServerHelper(port, "", parent)
{}

SslServerHelper::SslServerHelper(quint16 port, QString allowedPath, QObject *parent)
    : QObject(parent)
    , port(port)
    , pWebSocketServer(nullptr)
    , allowedPath(allowedPath)
    , keepalive_timer(new QTimer(this))
{
    certFileFound = false;
    keyFileFound = false;
    qDebug() << server_name << "Check cert files";
    pWebSocketServer = new QWebSocketServer(QStringLiteral("FastECU Broker Server"),
                                            QWebSocketServer::SecureMode,
                                            this);
    QSslConfiguration sslConfiguration;
    QFile certFile(QStringLiteral("localhost.cert"));
    QFile keyFile(QStringLiteral("localhost.key"));
    if (certFile.open(QIODevice::ReadOnly))
        certFileFound = true;
    if (keyFile.open(QIODevice::ReadOnly))
        keyFileFound = true;
    QSslCertificate certificate(&certFile, QSsl::Pem);
    QSslKey sslKey(&keyFile, QSsl::Rsa, QSsl::Pem);
    certFile.close();
    keyFile.close();
    sslConfiguration.setPeerVerifyMode(QSslSocket::VerifyNone);
    sslConfiguration.setLocalCertificate(certificate);
    sslConfiguration.setPrivateKey(sslKey);
    pWebSocketServer->setSslConfiguration(sslConfiguration);
    pWebSocketServer->setMaxPendingConnections(1);

    connect(this, &SslServerHelper::peerConnected, this, &SslServerHelper::start_keepalive, Qt::QueuedConnection);
    connect(this, &SslServerHelper::peerDisconnected, this, &SslServerHelper::stop_keepalive, Qt::QueuedConnection);

    connect(keepalive_timer, &QTimer::timeout, this, &SslServerHelper::send_keepalive, Qt::QueuedConnection);
}

SslServerHelper::~SslServerHelper()
{
    this->stop();
    pWebSocketServer->deleteLater();
}

bool SslServerHelper::start(void)
{
    //Start a server
    bool r = pWebSocketServer->listen(QHostAddress::Any, port);
    if (r)
    {
        qDebug() << server_name << "server listening on port" << port;
        connect(pWebSocketServer, &QWebSocketServer::newConnection,
                this, &SslServerHelper::onNewConnection, Qt::QueuedConnection);
        connect(pWebSocketServer, &QWebSocketServer::sslErrors,
                this, &SslServerHelper::onSslErrors, Qt::QueuedConnection);
    }
    else
    {
        qDebug() << server_name << "failed to start server on port" << port;
        emit log("Failed to open port "+QString::number(port));
    }
    return r;
}

void SslServerHelper::stop(void)
{
    if (pWebSocketServer->isListening())
    {
        qDebug() << server_name << "server stopping on port" << port;
        pWebSocketServer->close();
    }
}

void SslServerHelper::onNewConnection()
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
        emit log(server_name + " Peer "+p_addr+":"+p_port+" tries to connect at invalid path");
        qDebug() << server_name << "Peer " << p_addr << p_port << "tries to connect at invalid path";
        delete pSocket;
        return;
    }

    emit log(server_name + " Peer connected: "+p_addr+":"+p_port);
    qDebug() << server_name << "Peer connected:" << p_addr << p_port;
    //Qt::DirectConnection mode calls slot immediately after signal emittig
    connect(pSocket, &QWebSocket::textMessageReceived,
            this, &SslServerHelper::processTextMessage, Qt::DirectConnection);
    connect(pSocket, &QWebSocket::binaryMessageReceived,
            this, &SslServerHelper::processBinaryMessage, Qt::DirectConnection);
    connect(pSocket, &QWebSocket::disconnected,
            this, &SslServerHelper::socketDisconnected, Qt::QueuedConnection);

    peer = pSocket;
    pings_sequently_missed = 0;
    connect(peer, &QWebSocket::pong, this, &SslServerHelper::pong);
    emit peerConnected("");
}

void SslServerHelper::start_keepalive()
{
    if (peer != nullptr &&
        keepalive_interval > 0 &&
        !keepalive_timer->isActive())
    {
        qDebug() << server_name << "Starting keepalives";
        keepalive_timer->start(keepalive_interval);
    }
    else
    {
        qDebug() << server_name << "Cannot start keepalive.";
    }
}

static const QByteArray keepalive_payload = QByteArray::fromHex("5468757320646F20776520696E766F6B6520746865204D616368696E6520476F642E205468757320646F207765206D616B652077686F6C652074686174207768696368207761732073756E64657265642E");
static const int keepalive_palyload_len = keepalive_payload.length();

void SslServerHelper::send_keepalive()
{
    if (pings_sequently_missed == pings_sequently_missed_limit)
    {
        qDebug() << server_name << "Missed keepalives limit exceeded. Assume the client is disconnected.";
        emit log(server_name + " Missed keepalives limit exceeded. Assume the client is disconnected.");
        if (peer)
            peer->close();
        return;
    }
    QByteArray payload;
    if (keepalive_palyload_len > 0)
    {
        payload.append(keepalive_payload[keepalive_payload_pos]);
        //From 0 to keepalive payload length
        keepalive_payload_pos = (keepalive_payload_pos + 1) % keepalive_palyload_len;
    }
    ping(payload);
    pings_sequently_missed++;
}

void SslServerHelper::stop_keepalive()
{
    qDebug() << server_name << "stopping keepalives";
    keepalive_timer->stop();
}

void SslServerHelper::ping(const QByteArray &payload)
{
    peer->ping(payload);
}

void SslServerHelper::pong(quint64 elapsedTime, const QByteArray &payload)
{
    pings_sequently_missed = 0;
}

//Message is received from network, send it to broker
void SslServerHelper::processTextMessage(QString message)
{
    //qDebug() << server_name << "Peer sent text message";
    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    if (pClient !=nullptr)
    {
        //qDebug() << server_name << "Sending message to broker";
        emit sendTextMessageToBroker(message);
    }
}

//Message is received from network, send it to broker
void SslServerHelper::processBinaryMessage(QByteArray message)
{
    //qDebug() << server_name << "Peer sent binary message";
    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    if (pClient !=nullptr)
    {
        //qDebug() << server_name << "Sending message to broker";
        emit sendBinaryMessageToBroker(message);
    }
}

//Message is receiver from broker, send it to network
void SslServerHelper::receiveTextMessageFromBroker(QString message)
{
    //qDebug() << server_name << "Received text message from broker";
    if (peer != nullptr)
    {
        //qDebug() << server_name << "Sending text message to peer";
        peer->sendTextMessage(message);
    }
}

//Message is receiver from broker, send it to network
void SslServerHelper::receiveBinaryMessageFromBroker(QByteArray &message)
{
    //qDebug() << server_name << "Received binary message from broker";
    if (peer != nullptr)
    {
        //qDebug() << server_name << "Sending binary message to peer";
        peer->sendBinaryMessage(message);
    }
}

void SslServerHelper::socketDisconnected()
{
    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    if (pClient)
    {
        QString p_addr = QHostAddressToString(pClient->peerAddress());
        QString p_port = QString::number(pClient->peerPort());
        emit log(server_name + " Peer disconnected: "+p_addr+":"+p_port);
        qDebug() << server_name << "Peer disconnected:" << p_addr << p_port;

        peer = nullptr;
        pClient->deleteLater();
        emit peerDisconnected("");
    }
}

void SslServerHelper::onSslErrors(const QList<QSslError> &errors)
{
    emit log(server_name + "SSL server errors occurred");
    qDebug() << server_name << "Ssl errors occurred" << errors;
}

bool SslServerHelper::isSslCertFileFound()
{
    return certFileFound;
}

bool SslServerHelper::isSslKeyFileFound()
{
    return keyFileFound;
}

//================================================

SslServer::SslServer(quint16 port, QString allowedPath, QObject *parent)
    : QObject(parent)
    , server(new SslServerHelper(port, allowedPath, this))
{
    connect(this, &SslServer::enableKeepalive,
            this, &SslServer::enable_keepalive, Qt::QueuedConnection);
    connect(this,         &SslServer::receiveBinaryMessageFromBroker,
            server, &SslServerHelper::receiveBinaryMessageFromBroker, Qt::DirectConnection);
    connect(this,         &SslServer::receiveTextMessageFromBroker,
            server, &SslServerHelper::receiveTextMessageFromBroker, Qt::DirectConnection);
    connect(this,         &SslServer::setKeepaliveInterval,
            server, &SslServerHelper::set_keepalive_interval, Qt::QueuedConnection);
    connect(this,         &SslServer::setKeepaliveMissedLimit,
            server, &SslServerHelper::set_keepalive_missed_limit, Qt::QueuedConnection);
    connect(this,         &SslServer::setServerName,
            server, &SslServerHelper::set_server_name, Qt::QueuedConnection);
    connect(this,         &SslServer::start,
            server, &SslServerHelper::start, Qt::QueuedConnection);
    connect(this,         &SslServer::stop,
            server, &SslServerHelper::stop, Qt::QueuedConnection);

    connect(server, &SslServerHelper::log,
            this,         &SslServer::log, Qt::QueuedConnection);
    connect(server, &SslServerHelper::peerConnected,
            this,         &SslServer::peerConnected, Qt::QueuedConnection);
    connect(server, &SslServerHelper::peerDisconnected,
            this,         &SslServer::peerDisconnected, Qt::QueuedConnection);
    connect(server, &SslServerHelper::sendBinaryMessageToBroker,
            this,         &SslServer::sendBinaryMessageToBroker, Qt::DirectConnection);
    connect(server, &SslServerHelper::sendTextMessageToBroker,
            this,         &SslServer::sendTextMessageToBroker, Qt::DirectConnection);
}

SslServer::~SslServer()
{
    server->deleteLater();
}

void SslServer::enable_keepalive(bool enable)
{
    if (enable)
    {
        server->start_keepalive();
    }
    else
    {
        server->stop_keepalive();
    }
}
