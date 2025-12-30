#include "connectmanager.h"
#include "config.h"

#include <QDebug>


ConnectManager& ConnectManager::getInstance()
{
    static ConnectManager connect;
    connect._socket = new QTcpSocket();
    if (!connect._isCreate) {
        connect._socket->connectToHost(HOSTNAME, PORT);
        if (connect._socket->state() != QAbstractSocket::ConnectingState) {
            qDebug() << "连接失败";
        }
        connect._isCreate = true;
    }

    return connect;
}

QTcpSocket *ConnectManager::getSocket()
{
    return _socket;
}

ConnectManager::~ConnectManager()
{
    delete _socket;
}

ConnectManager::ConnectManager():_isCreate(false)
{

}


