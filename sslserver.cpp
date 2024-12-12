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

SslServer::SslServer(quint16 port, QObject *parent)
    : SslServer(port, "", parent)
{}

SslServer::SslServer(quint16 port, QString allowedPath, QObject *parent)
    : QObject(parent)
    , port(port)
    , pWebSocketServer(nullptr)
    , allowedPath(allowedPath)
    , keepalive_timer(new QTimer(this))
{
    certFileFound = false;
    keyFileFound = false;
    qDebug() << "Check cert files";
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

    connect(this, &SslServer::peerConnected, this, &SslServer::start_keepalive);
    connect(this, &SslServer::peerDisconnected, this, &SslServer::stop_keepalive);

    connect(keepalive_timer, &QTimer::timeout, this, &SslServer::send_keepalive);
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
    if (pWebSocketServer->isListening())
    {
        qDebug() << "SslServer: server stopping on port" << port;
        pWebSocketServer->close();
    }
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
    pings_sequently_missed = 0;
    connect(peer, &QWebSocket::pong, this, &SslServer::pong);
    emit peerConnected("");
}

void SslServer::start_keepalive()
{
    if (peer != nullptr &&
        keepalive_interval > 0 &&
        !keepalive_timer->isActive())
    {
        qDebug() << "SslServer: Starting keepalives";
        keepalive_timer->start(keepalive_interval);
    }
    else
    {
        qDebug() << "SslServer: Cannot start keepalive.";
    }
}

static const QByteArray keepalive_payload = QByteArray::fromHex("5468757320646F20776520696E766F6B6520746865204D616368696E6520476F642E205468757320646F207765206D616B652077686F6C652074686174207768696368207761732073756E64657265642E");
static const int keepalive_palyload_len = keepalive_payload.length();

void SslServer::send_keepalive()
{
    if (pings_sequently_missed == pings_sequently_missed_limit)
    {
        qDebug() << "Missed keepalives limit exceeded. Assume the client is disconnected.";
        emit log("Missed keepalives limit exceeded. Assume the client is disconnected.");
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

void SslServer::stop_keepalive()
{
    qDebug() << "SslServer: stopping keepalives";
    keepalive_timer->stop();
}

void SslServer::ping(const QByteArray &payload)
{
    peer->ping(payload);
}

void SslServer::pong(quint64 elapsedTime, const QByteArray &payload)
{
    pings_sequently_missed = 0;
}

//Message is received from network, send it to broker
void SslServer::processTextMessage(QString message)
{
    //qDebug() << "SslServer: Peer sent text message";
    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    if (pClient !=nullptr)
    {
        //qDebug() << "SslServer: Sending message to broker";
        emit sendTextMessageToBroker(message);
    }
}

//Message is received from network, send it to broker
void SslServer::processBinaryMessage(QByteArray message)
{
    //qDebug() << "SslServer: Peer sent binary message";
    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    if (pClient !=nullptr)
    {
        //qDebug() << "SslServer: Sending message to broker";
        emit sendBinaryMessageToBroker(message);
    }
}

//Message is receiver from broker, send it to network
void SslServer::receiveTextMessageFromBroker(QString message)
{
    //qDebug() << "SslServer: Received text message from broker";
    if (peer != nullptr)
    {
        //qDebug() << "SslServer: Sending text message to peer";
        peer->sendTextMessage(message);
    }
}

//Message is receiver from broker, send it to network
void SslServer::receiveBinaryMessageFromBroker(QByteArray &message)
{
    //qDebug() << "SslServer: Received binary message from broker";
    if (peer != nullptr)
    {
        //qDebug() << "SslServer: Sending binary message to peer";
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

void SslServer::onSslErrors(const QList<QSslError> &errors)
{
    emit log("SSL server errors occurred");
    qDebug() << "SslServer: Ssl errors occurred" << errors;
}

bool SslServer::isSslCertFileFound()
{
    return certFileFound;
}

bool SslServer::isSslKeyFileFound()
{
    return keyFileFound;
}
