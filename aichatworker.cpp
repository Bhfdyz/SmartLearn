#include "aichatworker.h"
#include <QDebug>
#include <QAbstractSocket>

AIChatWorker::AIChatWorker(QObject *parent)
    : QObject(parent)
    , _socket(nullptr)
{
    _socket = new QTcpSocket(this);
}

AIChatWorker::~AIChatWorker()
{
    if (_socket) {
        _socket->abort();
        _socket->deleteLater();
    }
}

void AIChatWorker::sendRequest(const QString &jsonData, const QString &host, quint16 port)
{
    qDebug() << "AIChatWorker: 开始发送请求";

    if (!_socket) {
        emit requestError("Socket未初始化");
        return;
    }

    // 如果socket已连接，先断开
    if (_socket->state() == QAbstractSocket::ConnectedState) {
        _socket->abort();
    }

    // 连接服务器
    _socket->connectToHost(host, port);

    if (!_socket->waitForConnected(5000)) {
        QString error = "连接服务器失败: " + _socket->errorString();
        qDebug() << "AIChatWorker:" << error;
        emit requestError(error);
        return;
    }

    qDebug() << "AIChatWorker: 已连接到服务器" << host << ":" << port;

    // 发送数据
    QByteArray data = jsonData.toUtf8();
    _socket->write(data);
    _socket->flush();

    qDebug() << "AIChatWorker: 数据已发送，等待响应...";

    // 等待响应（30秒超时）
    if (_socket->waitForReadyRead(60000)) {
        QByteArray response = _socket->readAll();
        qDebug() << "AIChatWorker: 收到响应，大小:" << response.size();
        emit requestFinished(response);
    } else {
        QString error = "请求超时";
        qDebug() << "AIChatWorker:" << error;
        emit requestError(error);
    }

    // 请求完成后断开连接
    _socket->abort();
}
