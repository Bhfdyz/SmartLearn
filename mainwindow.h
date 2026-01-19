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
                               const QString &message, const QStringList &knowledgeList = QStringList(),
                               const QString &learningGoal = "");  // 发送知识库响应

    // ========== AI对话相关 ==========
    void handleAIChatRequest(const QJsonObject &json, QTcpSocket *socket);  // 处理AI对话请求
    void handleGetAIChatHistoryRequest(const QJsonObject &json, QTcpSocket *socket);  // 获取对话历史
    void handleGetSessionListRequest(const QJsonObject &json, QTcpSocket *socket);  // 获取会话列表
    void handleDeleteSessionRequest(const QJsonObject &json, QTcpSocket *socket);  // 删除会话
    void sendAIChatResponse(QTcpSocket *socket, const QString &status, const QString &message,
                            const QString &content = "", const QString &sessionId = "");  // 发送AI响应

    // ========== 学习路径相关 ==========
    void handleGeneratePathRequest(const QJsonObject &json, QTcpSocket *socket);  // 生成学习路径
    void handleGetPathListRequest(const QJsonObject &json, QTcpSocket *socket);  // 获取路径列表
    void handleGetPathDetailRequest(const QJsonObject &json, QTcpSocket *socket);  // 获取路径详情
    void handleDeletePathRequest(const QJsonObject &json, QTcpSocket *socket);  // 删除路径
    void handleUpdatePathProgressRequest(const QJsonObject &json, QTcpSocket *socket);  // 更新路径进度（阶段级别）
    void handleUpdateStepProgressRequest(const QJsonObject &json, QTcpSocket *socket);  // 更新步骤进度（步骤级别）
    void sendPathResponse(QTcpSocket *socket, const QString &status, const QString &message,
                          const QJsonObject &data = QJsonObject());  // 发送路径响应

    // ========== 学习资源相关 ==========
    void handleGenerateResourcesRequest(const QJsonObject &json, QTcpSocket *socket);  // AI生成资源推荐
    void handleGetResourcesRequest(const QJsonObject &json, QTcpSocket *socket);  // 获取资源列表
    void sendResourcesResponse(QTcpSocket *socket, const QString &status, const QString &message,
                               const QJsonArray &resources = QJsonArray());  // 发送资源响应

    // ========== 验证方法 ==========
    bool validateUsername(const QString &username);
    bool validatePassword(const QString &password);
    bool validateEmail(const QString &email);
    bool validatePhone(const QString &phone);

};
#endif // MAINWINDOW_H
