#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "databasemanager.h"

#include <QMainWindow>
#include <QTcpServer>
#include <QSqlDatabase>
#include <QJsonObject>
#include <QTcpSocket>
#include <QRegularExpression>
#include <QJsonArray>
#include <QStringList>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    QTcpServer *_server;
    QSqlDatabase _db;

private slots:
    void SlotNewClient();
    void SlotReadFromClient();

private:
    // ========== 登录相关 ==========
    void handleLoginRequest(const QJsonObject &json, QTcpSocket *socket);

    // ========== 注册相关 ==========
    void handleRegisterRequest(const QJsonObject &json, QTcpSocket *socket);
    void sendRegisterResponse(QTcpSocket *socket, const QString &status,
                             int error_code, const QString &message, int user_id = 0);

    // ========== 知识库相关 ==========
    void handleSaveKnowledgeRequest(const QJsonObject &json, QTcpSocket *socket);  // 保存知识库
    void handleGetKnowledgeRequest(const QJsonObject &json, QTcpSocket *socket);    // 获取知识库
    void sendKnowledgeResponse(QTcpSocket *socket, const QString &status,
                               const QString &message, const QStringList &knowledgeList = QStringList());  // 发送知识库响应

    // ========== 验证方法 ==========
    bool validateUsername(const QString &username);
    bool validatePassword(const QString &password);
    bool validateEmail(const QString &email);
    bool validatePhone(const QString &phone);

};
#endif // MAINWINDOW_H
