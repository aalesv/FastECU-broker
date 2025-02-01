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

SslServer::SslServer(quint16 port, QString password, QObject *parent)
    : QObject(parent)
    , port(port)
    , pWebSocketServer(nullptr)
    , password(password)
{
    certFileFound = false;
    keyFileFound = false;
    qDebug() << serverName << "Check cert files";
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
        qDebug() << serverName << "server listening on port" << port;
        connect(pWebSocketServer, &QWebSocketServer::newConnection,
                this, &SslServer::onNewConnection, Qt::DirectConnection);
        connect(pWebSocketServer, &QWebSocketServer::sslErrors,
                this, &SslServer::onSslErrors,Qt::DirectConnection);
    }
    else
    {
        qDebug() << serverName << "failed to start server on port" << port;
        emit log("Failed to open port "+QString::number(port));
    }
    return r;
}

void SslServer::stop(void)
{
    if (pWebSocketServer->isListening())
    {
        qDebug() << serverName << "server stopping on port" << port;
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

    QString p_addr = QHostAddressToString(pSocket->peerAddress());
    QString p_port = QString::number(pSocket->peerPort());
    QUrl reqUrl = pSocket->requestUrl();
    QString p_path = reqUrl.path();
    QString p_pass = QString(pSocket->request().rawHeader(webSocketPasswordHeader.toUtf8()));


    //Path must be unique
    if (auto plist = peers.peers(reqUrl.path()); plist.size() > 0)
    {
        qDebug() << serverName << "Peer" << p_addr << p_port
                 <<"is trying to connect to already connected path"
                 << reqUrl.path();
        check_connection(plist);
        delete pSocket;
        return;
    }

    if (p_pass != password)
    {
        emit log(serverName+" Peer "+p_addr+":"+p_port+" incorrect password");
        qDebug() << serverName << "Peer " << p_addr << p_port << "incorrect password";
        delete pSocket;
        return;
    }

    emit log(serverName+" Peer connected: "+p_addr+":"+p_port+p_path);
    qDebug() << serverName << "Peer connected:" << p_addr << p_port << p_path;
    //Qt::DirectConnection mode calls slot immediately after signal emittig
    connect(pSocket, &QWebSocket::textMessageReceived,
            this, &SslServer::processTextMessage, Qt::DirectConnection);
    connect(pSocket, &QWebSocket::binaryMessageReceived,
            this, &SslServer::processBinaryMessage, Qt::DirectConnection);
    connect(pSocket, &QWebSocket::disconnected,
            this, &SslServer::socketDisconnected, Qt::DirectConnection);

    Peer *peer = peers.append(pSocket);
    connect(pSocket, &QWebSocket::pong, this, &SslServer::pong);
    connect(peer->keepalive_timer, &QTimer::timeout, this, [this](){
        QTimer *timer = qobject_cast<QTimer *>(sender());
        Peer *peer = qobject_cast<Peer *>(timer->parent());
        this->send_keepalive(peer);
    });
    emit peerConnected("");
}

void SslServer::start_keepalives()
{
    for (auto p : peers)
        start_keepalive(p);

    if (peers.size() == 0)
        qDebug() << serverName << "Cannot start keepalives - no peer present";
}

void SslServer::start_keepalive(Peer *peer)
{
    if (peer == nullptr)
        return;

    if (peer->socket() != nullptr &&
        keepalive_interval > 0 &&
        !peer->keepalive_timer->isActive())
    {
        qDebug() << serverName << "Starting keepalives"
                 << QHostAddressToString(peer->socket()->peerAddress())
                 << peer->socket()->peerPort();
        peer->keepalive_timer->start(keepalive_interval);
    }
    else
    {
        QString msg;
        QString p_addr = (peer->socket()) ?
            QHostAddressToString(peer->socket()->peerAddress())
            :
            "";
        int p_port = (peer->socket()) ? peer->socket()->peerPort() : 0;
        if (peer->socket() == nullptr)
            msg = "no peer present";
        if (keepalive_interval <= 0)
            msg = "keepalives disabled";
        if (peer->keepalive_timer->isActive())
            msg = "keepalives are already active";
        qDebug() << serverName << "Cannot start keepalive"
                 << p_addr << p_port << msg;
    }
}

static const QByteArray keepalive_payload = QByteArray::fromHex("5468757320646F20776520696E766F6B6520746865204D616368696E6520476F642E205468757320646F207765206D616B652077686F6C652074686174207768696368207761732073756E64657265642E");
static const int keepalive_palyload_len = keepalive_payload.length();

void SslServer::send_keepalive(Peer *peer)
{
    if (peer == nullptr)
        return;

    if (peer->pings_sequently_missed == pings_sequently_missed_limit)
    {
        qDebug() << serverName << "Missed keepalives limit exceeded. Assume the client is disconnected.";
        emit log(serverName+" Missed keepalives limit exceeded. Assume the client is disconnected.");
        peer->socket()->close();
        return;
    }
    QByteArray payload;
    if (keepalive_palyload_len > 0)
    {
        payload.append(keepalive_payload[keepalive_payload_pos]);
        //From 0 to keepalive payload length
        keepalive_payload_pos = (keepalive_payload_pos + 1) % keepalive_palyload_len;
    }
    ping(peer->socket(), payload);
    peer->pings_sequently_missed++;
}

void SslServer::stop_keepalives()
{
    for (auto peer : peers)
        stop_keepalive(peer);
}

void SslServer::stop_keepalive(Peer *peer)
{
    if (peer == nullptr)
        return;

    if (peer->hung_connection_flag &&
         keepalive_interval > 0)
    {
        if (peer->keepalive_timer->isActive())
        {
            qDebug() << serverName << "Not stopping keepalives"
                     << QHostAddressToString(peer->socket()->peerAddress())
                     << peer->socket()->peerPort();
        }
    }
    else
    {
        if (peer->keepalive_timer->isActive())
        {
            qDebug() << serverName << "Stopping keepalives"
                     << QHostAddressToString(peer->socket()->peerAddress())
                     << peer->socket()->peerPort();
            peer->keepalive_timer->stop();
        }
    }
}

void SslServer::ping(QWebSocket *pSocket, const QByteArray &payload)
{
    pSocket->ping(payload);
}

void SslServer::pong(quint64 elapsedTime, const QByteArray &payload)
{
    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    Peer *peer = peers.peer(pClient);
    peer->pings_sequently_missed = 0;
    //connection_restored(peer);
    //qDebug() << serverName << "Pong from"
    //         << QHostAddressToString(peer->socket()->peerAddress())
    //         << peer->socket()->peerPort();
}

void SslServer::check_connections()
{
    auto now = chrono_clock::now();
    //Convert to milliseconds
    int interval = std::chrono::duration_cast<std::chrono::milliseconds>
                   (now - last_connections_check).count();
    if (interval > periodic_connections_check_interval)
    {
        //qDebug() << serverName << "Running periodic connections check";
        check_connection(peers);
        last_connections_check = chrono_clock::now();
    }
}

void SslServer::check_connection(PeerStorage &plist)
{
    for (auto p : plist)
        check_connection(p);
}

void SslServer::check_connection(Peer *peer)
{
    if (peer == nullptr)
        return;

    if (!peer->hung_connection_flag)
    {
        //Check active connection and mark it as hung if needed
        auto now = chrono_clock::now();
        //Convert to milliseconds
        int interval = std::chrono::duration_cast<std::chrono::milliseconds>
                        (now - peer->last_input_packet_time).count();
        QWebSocket *s = peer->socket();
        //qDebug() << serverName << "check_connection"
        //         << QHostAddressToString(s->peerAddress())
        //         << s->peerPort() << s->requestUrl().path()
        //         << interval;
        if (interval > hung_connection_interval)
        {
            qDebug() << serverName << "Connection hung"
                     << QHostAddressToString(peer->socket()->peerAddress())
                     << peer->socket()->peerPort();
            peer->hung_connection_flag = true;
            if (keepalive_interval > 0)
                start_keepalive(peer);
            emit connectionHung();
        }
    }
    //If connection is marked as hung,
    //keepalives are enabled but not active,
    //then start keepalives
    else if (keepalive_interval > 0 &&
            !peer->keepalive_timer->isActive())
    {
        qDebug() << serverName << "Found hung connection with stopped keepalives "
                 "while keepalives are enabled"
                 << QHostAddressToString(peer->socket()->peerAddress())
                 << peer->socket()->peerPort();
        start_keepalive(peer);
    }
}

void SslServer::connection_restored(Peer *peer)
{
    if (peer == nullptr)
        return;

    peer->last_input_packet_time = chrono_clock::now();
    if (peer->hung_connection_flag)
    {
        qDebug() << serverName << "Hung connection has been restored"
                 << QHostAddressToString(peer->socket()->peerAddress())
                 << peer->socket()->peerPort();
        peer->hung_connection_flag = false;
        emit connectionRestored();
    }
}

//Message is received from network, send it to broker
void SslServer::processTextMessage(QString message)
{
    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    //qDebug() << serverName << "Peer sent text message"
    //         << QHostAddressToString(pClient->peerAddress())
    //         << pClient->peerPort()
    //         << pClient->requestUrl().path();
    if (pClient !=nullptr)
    {
        connection_restored(peers.peer(pClient));
        //qDebug() << serverName << "Sending message to broker";
        emit sendTextMessageToBroker(message, pClient->requestUrl().path());
    }
}

//Message is received from network, send it to broker
void SslServer::processBinaryMessage(QByteArray message)
{
    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    //qDebug() << serverName << "Peer sent binary message" << pClient->requestUrl().path();
    if (pClient !=nullptr)
    {
        connection_restored(peers.peer(pClient));
        //qDebug() << serverName << "Sending message to broker";
        emit sendBinaryMessageToBroker(message, pClient->requestUrl().path());
    }
}

//Message is receiver from broker, send it to network
void SslServer::receiveTextMessageFromBroker(QString message, QString path)
{
    check_connections();
    //qDebug() << serverName << "Received text message from broker" << path;
    for(auto pSocket : peers.sockets(path))
    {
        if (pSocket != nullptr)
        {
            //check_connection(peers.peer(pSocket));
            //qDebug() << serverName << "Sending text message to peer"
            //         << QHostAddressToString(pSocket->peerAddress())
            //         << pSocket->peerPort();
            pSocket->sendTextMessage(message);
        }
    }
}

//Message is receiver from broker, send it to network
void SslServer::receiveBinaryMessageFromBroker(QByteArray &message, QString path)
{
    check_connections();
    //qDebug() << serverName << "Received binary message from broker" << path;
    for(auto pSocket : peers.sockets(path))
    {
        if (pSocket != nullptr)
        {
            //check_connection(peers.peer(pSocket));
            //qDebug() << serverName << "Sending binary message to peer"
            //         << QHostAddressToString(pSocket->peerAddress())
            //         << pSocket->peerPort();
            pSocket->sendBinaryMessage(message);
        }
    }
}

void SslServer::socketDisconnected()
{
    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    if (pClient)
    {
        QString p_addr = QHostAddressToString(pClient->peerAddress());
        QString p_port = QString::number(pClient->peerPort());
        emit log(serverName+" Peer disconnected: "+p_addr+":"+p_port);
        qDebug() << serverName << "Peer disconnected:" << p_addr << p_port;

        pClient->deleteLater();
        emit peerDisconnected("");
    }
}

void SslServer::onSslErrors(const QList<QSslError> &errors)
{
    emit log(serverName+" SSL server errors occurred");
    qDebug() << serverName << "Ssl errors occurred" << errors;
}

bool SslServer::isSslCertFileFound()
{
    return certFileFound;
}

bool SslServer::isSslKeyFileFound()
{
    return keyFileFound;
}

bool SslServer::isPeerConnected()
{
    return (peers.size() > 0);
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
    server.setName("Server:");
    client.setName("Client:");
    int k_int = 0;
    if (keepalive_enabled)
        k_int = keepalive_interval;
    server.set_keepalive_interval(k_int);
    client.set_keepalive_interval(k_int);
    server.set_keepalive_missed_limit(keepalive_missed_limit);
    client.set_keepalive_missed_limit(keepalive_missed_limit);
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

    connect(&server, &SslServer::connectionHung,
            this, &Broker::connection_hung, Qt::QueuedConnection);
    connect(&client, &SslServer::connectionHung,
            this, &Broker::connection_hung, Qt::QueuedConnection);

    connect(&server, &SslServer::connectionRestored,
            this, &Broker::connection_restored, Qt::QueuedConnection);
    connect(&client, &SslServer::connectionRestored,
            this, &Broker::connection_restored, Qt::QueuedConnection);
}

Broker::~Broker()
{
    this->stop();
}

bool Broker::start(void)
{
    bool ssl_is_supported = QSslSocket::supportsSsl();
    if (! ssl_is_supported)
    {
        qDebug() << "ERROR: SSL is NOT supported!";
        emit log("ERROR: SSL libaries not found! No further work possible. "
                 "Fix the error and restart the application.");
    }

    bool r = ssl_is_supported && server.start() && client.start();
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

void Broker::server_connected(QString message)
{
    enable_keepalive(keepalive_enabled);
    emit serverConnected(message);
}

void Broker::server_disconnected(QString message)
{
    enable_keepalive(keepalive_enabled);
    emit serverDisconnected(message);
}

void Broker::client_connected(QString message)
{
    enable_keepalive(keepalive_enabled);
    emit clientConnected(message);
}

void Broker::client_disconnected(QString message)
{
    enable_keepalive(keepalive_enabled);
    emit clientDisconnected(message);
}

void Broker::receiveTextMessageFromSslServer(QString message, QString path)
{
    //qDebug() << "Broker: received text message from server";
    //qDebug() << "Broker: sending text message to client";
    emit sendTextMessageToSslClient(message, path);
}

void Broker::receiveTextMessageFromSslClient(QString message, QString path)
{
    //qDebug() << "Broker: received text message from client";
    if (passClientTextMessage(message))
    {
        //qDebug() << "Broker: sending text message to server";
        emit sendTextMessageToSslServer(message, path);
    }
    else
    {
        qDebug() << "Broker: text message" << message << "filtered";
    }
}

void Broker::receiveBinaryMessageFromSslServer(QByteArray &message, QString path)
{
    //qDebug() << "Broker: received binary message from server";
    //qDebug() << "Broker: sending binary message to client";
    emit sendBinaryMessageToSslClient(message, path);
}

void Broker::receiveBinaryMessageFromSslClient(QByteArray &message, QString path)
{
    //qDebug() << "Broker: received binary message from client";
    //qDebug() << "Broker: sending binary message to server";
    emit sendBinaryMessageToSslServer(message, path);
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
    if (enable &&
        //Enable if only one peer connected
        //client only or server only
        (server.isPeerConnected() ^ client.isPeerConnected())
        )
    {
        qDebug() << "Broker: Enabling keepalives";
        server.set_keepalive_interval(keepalive_interval);
        client.set_keepalive_interval(keepalive_interval);
        server.start_keepalives();
        client.start_keepalives();
    }
    else
    {
        if (enable)
        {
            qDebug() << "Broker: Stopping currently active keepalives";
            server.set_keepalive_interval(keepalive_interval);
            client.set_keepalive_interval(keepalive_interval);
        }
        else
        {
            qDebug() << "Broker: Disabling keepalives";
            server.set_keepalive_interval(0);
            client.set_keepalive_interval(0);
        }
        server.stop_keepalives();
        client.stop_keepalives();
    }
}

void Broker::connection_hung()
{
}

void Broker::connection_restored()
{
    SslServer *srv = qobject_cast<SslServer *>(sender());
    enable_keepalive(keepalive_enabled);
}
