// This is an open source non-commercial project. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "peerstorage.h"

Peer::Peer(QWebSocket *pSocket)
    : QObject(pSocket)
    , pSocket(pSocket)
    , keepalive_timer(new QTimer(this))
{}

Peer::~Peer()
{
    if (storage != nullptr)
    {
        storage->removeOne(this);
    }
}

//==================================================
PeerStorage::~PeerStorage()
{
    for (int i = 0; i < size(); i++)
    {
        auto s = at(i);
        s->storage = nullptr;
        s->deleteLater();
    }
}

Peer* PeerStorage::append(QWebSocket *s)
{
    Peer *p = new Peer(s);
    p->storage = this;
    append(p);
    return p;
}

bool PeerStorage::contains(QString path)
{
    for (int i = 0; i < size(); i++)
    {
        if (at(i)->socket()->requestUrl().path() == path)
            return true;
    }
    return false;
}

QVector<QWebSocket*> PeerStorage::sockets(QString path) const
{
    QVector<QWebSocket*> r;
    for (int i = 0; i < size(); i++)
    {
        QWebSocket* s = at(i)->socket();
        if (s->requestUrl().path() == path)
        {
            r.append(s);
        }
    }
    return r;
}

QVector<Peer*> PeerStorage::peers(QString path) const
{
    QVector<Peer*> r;
    for (int i = 0; i < size(); i++)
    {
        Peer* p = at(i);
        if (p->socket()->requestUrl().path() == path)
        {
            r.append(p);
        }
    }
    return r;
}

Peer* PeerStorage::peer(QWebSocket *s) const
{
    for (int i = 0; i < size(); i++)
    {
        Peer* p = at(i);
        if (p->socket() == s)
        {
            return p;
        }
    }
    return nullptr;
}
