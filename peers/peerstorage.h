// This is an open source non-commercial project. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#ifndef PEERSTORAGE_H
#define PEERSTORAGE_H

#include <QObject>
#include <QWebSocket>
#include <QTimer>

using chrono_clock = std::chrono::high_resolution_clock;

class PeerStorage;

class Peer : public QObject
{
    Q_OBJECT
public:
    explicit Peer(QWebSocket *pSocket);
    ~Peer();

    int pings_sequently_missed = 0;
    std::chrono::time_point<chrono_clock> last_input_packet_time =
                            chrono_clock::now();
    bool hanged_connection_flag = false;
    QTimer *keepalive_timer;

    QWebSocket* socket() { return pSocket; }

private:
    QWebSocket *pSocket = nullptr;
    PeerStorage *storage = nullptr;

signals:

friend class PeerStorage;
};

class PeerStorage : public QVector<Peer *>
{
    //Need to use parent's overloaded functions
    using QVector::QVector;
    using QVector::append;
    using QVector::contains;
public:
    //PeerStorage();
    ~PeerStorage();

    Peer* append(QWebSocket *s);
    bool contains(QString path);
    QVector<QWebSocket*> sockets(QString path) const;
    QVector<Peer*> peers(QString path) const;
    Peer* peer(QWebSocket *s) const;
};

#endif // PEERSTORAGE_H
