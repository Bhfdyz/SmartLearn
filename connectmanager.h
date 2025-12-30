#ifndef CONNECTMANAGER_H
#define CONNECTMANAGER_H

#include <QTcpSocket>

class ConnectManager
{
public:
    static ConnectManager& getInstance();
    QTcpSocket* getSocket();
    ~ConnectManager();

private:
    ConnectManager();
    ConnectManager(const ConnectManager&) = delete;
    ConnectManager& operator=(const ConnectManager&) = delete;

    QTcpSocket *_socket;
    bool _isCreate;
};

#endif // CONNECTMANAGER_H
