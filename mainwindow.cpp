#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "config.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlQuery>
#include <QTcpSocket>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->textEdit->setReadOnly(true);

    DatabaseManager &manager = DatabaseManager::getInstance();
    _db = manager.getDatabase();
    QSqlQuery query(_db);

    _server = new QTcpServer(this);
    if (!_server->listen(QHostAddress(HOSTPORT), PORT)) {
        qDebug() << "连接失败";
    }
    connect(_server, &QTcpServer::newConnection, this, &MainWindow::SlotNewClient);

}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::SlotNewClient()
{
    if (_server->hasPendingConnections()) {
        QTcpSocket *socket = _server->nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead,
            this, &MainWindow::SlotReadFromClient);
    }
}

void MainWindow::SlotReadFromClient()
{
    // 解析json
    QTcpSocket *tmpsocket = qobject_cast<QTcpSocket*>(sender());
    QByteArray revData = tmpsocket->readAll();
    QJsonDocument jsondoc = QJsonDocument::fromJson(revData);
    QJsonObject jsonobj = jsondoc.object();
    QString type = jsonobj["type"].toString();

    if (type == LoginType) {
        QString user = jsonobj["user"].toString();
        QString password = jsonobj["password"].toString();
        qDebug() << user;
        qDebug() << password;

        //  登录验证
        if (user == "1234" && password == "1234") {
            QList<QTcpSocket*> tcpSocketClients = _server->findChildren<QTcpSocket*>();
            for (QTcpSocket* tmp : tcpSocketClients) {
                tmp->write("yes");
            }
        } else {
            QList<QTcpSocket*> tcpSocketClients = _server->findChildren<QTcpSocket*>();
            for (QTcpSocket* tmp : tcpSocketClients) {
                tmp->write("no");
            }
        }
    }
}
