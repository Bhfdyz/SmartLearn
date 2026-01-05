#include "connectmanager.h"
#include "config.h"

#include <QDebug>


ConnectManager& ConnectManager::getInstance()
{
    static ConnectManager connect;
    if (!connect._isCreate) {
        connect._socket = new QTcpSocket();
        connect._socket->connectToHost(HOSTNAME, PORT);
        if (!connect._socket->waitForConnected(3000)) {
            qDebug() << "连接失败: " << connect._socket->errorString();
        } else {
            qDebug() << "连接到服务器成功";
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


