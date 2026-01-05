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

    _server = new QTcpServer(this);
    if (!_server->listen(QHostAddress(HOSTPORT), PORT)) {
        qDebug() << "服务器启动失败";
    } else {
        qDebug() << "服务器监听端口: " << PORT;
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
    QTcpSocket *tmpsocket = qobject_cast<QTcpSocket*>(sender());
    QByteArray revData = tmpsocket->readAll();
    QJsonDocument jsondoc = QJsonDocument::fromJson(revData);
    QJsonObject jsonobj = jsondoc.object();
    QString type = jsonobj["type"].toString();

    if (type == LoginType) {
        handleLoginRequest(jsonobj, tmpsocket);
    }
    else if (type == RegisterType) {
        handleRegisterRequest(jsonobj, tmpsocket);
    }
    else if (type == SaveKnowledgeType) {
        handleSaveKnowledgeRequest(jsonobj, tmpsocket);
    }
    else if (type == GetKnowledgeType) {
        handleGetKnowledgeRequest(jsonobj, tmpsocket);
    }
}

// ========== 登录相关 ==========

void MainWindow::handleLoginRequest(const QJsonObject &json, QTcpSocket *socket)
{
    QString user = json["user"].toString();
    QString password = json["password"].toString();

    qDebug() << "收到登录请求 - 用户名:" << user;

    // 使用数据库验证
    if (DatabaseManager::getInstance().checkUserLogin(user, password)) {
        // 获取用户信息并更新登录时间
        User userInfo = DatabaseManager::getInstance().getUserByUsername(user);
        DatabaseManager::getInstance().updateUserLastLogin(userInfo.user_id);
        DatabaseManager::getInstance().logUserAction(userInfo.user_id, "login",
                                                       socket->peerAddress().toString());

        qDebug() << "用户登录成功: " << user;
        socket->write("yes");
    } else {
        qDebug() << "用户登录失败: " << user;
        socket->write("no");
    }
    socket->flush();
}

// ========== 注册相关 ==========

void MainWindow::handleRegisterRequest(const QJsonObject &json, QTcpSocket *socket)
{
    // 1. 解析数据
    QString username = json["username"].toString();
    QString password = json["password"].toString();
    QString email = json["email"].toString();
    QString phone = json["phone"].toString();
    QString grade = json["grade"].toString();
    QString major = json["major"].toString();
    QString roleStr = json["role"].toString();

    qDebug() << "收到注册请求 - 用户名:" << username;

    // 2. 验证数据格式
    qDebug() << "步骤1: 开始验证用户名";
    if (!validateUsername(username)) {
        qDebug() << "用户名验证失败";
        sendRegisterResponse(socket, "error", INVALID_USERNAME,
                            "用户名格式错误（4-20个字符，字母数字下划线）");
        return;
    }

    qDebug() << "步骤2: 开始验证密码";
    if (!validatePassword(password)) {
        qDebug() << "密码验证失败";
        sendRegisterResponse(socket, "error", INVALID_PASSWORD,
                            "密码强度不足（至少8位，包含字母和数字）");
        return;
    }

    qDebug() << "步骤3: 开始验证邮箱";
    if (!email.isEmpty() && !validateEmail(email)) {
        qDebug() << "邮箱验证失败";
        sendRegisterResponse(socket, "error", INVALID_EMAIL, "邮箱格式错误");
        return;
    }

    qDebug() << "步骤4: 开始验证手机号";
    if (!phone.isEmpty() && !validatePhone(phone)) {
        qDebug() << "手机号验证失败";
        sendRegisterResponse(socket, "error", INVALID_PHONE, "手机号格式错误");
        return;
    }

    // 3. 检查用户是否存在
    qDebug() << "步骤5: 检查用户是否存在";
    if (DatabaseManager::getInstance().userExists(username)) {
        qDebug() << "用户已存在";
        sendRegisterResponse(socket, "error", USERNAME_EXISTS,
                            "该用户名已被注册，请换一个试试");
        return;
    }

    qDebug() << "步骤6: 检查邮箱是否存在";
    if (!email.isEmpty() && DatabaseManager::getInstance().emailExists(email)) {
        qDebug() << "邮箱已存在";
        sendRegisterResponse(socket, "error", EMAIL_EXISTS, "该邮箱已被注册");
        return;
    }

    // 4. 构造用户对象
    qDebug() << "步骤7: 构造用户对象";
    User user;
    user.username = username;
    user.password_hash = DatabaseManager::getInstance().hashPassword(password);
    user.email = email;
    user.phone = phone;
    user.grade = grade;
    user.major = major;
    user.role = (roleStr == "admin") ? ROLE_ADMIN : ROLE_STUDENT;
    user.status = 1;

    // 5. 插入数据库
    qDebug() << "步骤8: 插入数据库";
    if (DatabaseManager::getInstance().insertUser(user)) {
        // 获取新插入的用户ID
        User insertedUser = DatabaseManager::getInstance().getUserByUsername(username);
        int newUserId = insertedUser.user_id;

        // 记录日志
        DatabaseManager::getInstance().logUserAction(
            newUserId,
            "register",
            socket->peerAddress().toString(),
            "User registered successfully: " + username
        );

        qDebug() << "用户注册成功 - 用户名:" << username << "ID:" << newUserId;

        sendRegisterResponse(socket, "success", REGISTER_SUCCESS,
                            "注册成功，请登录", newUserId);
    } else {
        qDebug() << "数据库插入失败";
        sendRegisterResponse(socket, "error", DATABASE_ERROR,
                            "注册失败，请稍后重试");
    }

    qDebug() << "handleRegisterRequest 结束";
}

void MainWindow::sendRegisterResponse(QTcpSocket *socket, const QString &status,
                                     int error_code, const QString &message, int user_id)
{
    QJsonObject json;
    json["type"] = "RegisterResponse";
    json["status"] = status;
    json["error_code"] = error_code;
    json["message"] = message;

    if (user_id > 0) {
        json["user_id"] = user_id;
    }

    QJsonDocument doc(json);
    QByteArray data = doc.toJson();

    qDebug() << "发送注册响应:" << status << message;
    qDebug() << "响应内容:" << data;

    socket->write(data);
    socket->flush();
    socket->waitForBytesWritten(1000);  // 等待1秒确保数据发送到缓冲区

    qDebug() << "响应发送完成，关闭socket";
    socket->disconnectFromHost();
}

// ========== 验证方法 ==========

bool MainWindow::validateUsername(const QString &username)
{
    // 长度检查
    if (username.length() < 4 || username.length() > 20) {
        return false;
    }

    // 格式检查：字母开头，只允许字母、数字、下划线
    QRegularExpression regex("^[a-zA-Z][a-zA-Z0-9_]*$");
    return regex.match(username).hasMatch();
}

bool MainWindow::validatePassword(const QString &password)
{
    // 长度检查
    if (password.length() < 8) {
        return false;
    }

    // 复杂度检查：必须包含字母和数字
    bool hasLetter = false;
    bool hasDigit = false;

    for (const QChar &ch : password) {
        if (ch.isLetter()) hasLetter = true;
        if (ch.isDigit()) hasDigit = true;
    }

    return hasLetter && hasDigit;
}

bool MainWindow::validateEmail(const QString &email)
{
    QRegularExpression regex("^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$");
    return regex.match(email).hasMatch();
}

bool MainWindow::validatePhone(const QString &phone)
{
    QRegularExpression regex("^1[3-9]\\d{9}$");
    return regex.match(phone).hasMatch();
}

// ========== 知识库相关 ==========

void MainWindow::handleSaveKnowledgeRequest(const QJsonObject &json, QTcpSocket *socket)
{
    QString username = json["username"].toString();
    QString learning_goal = json["learning_goal"].toString();
    QJsonArray knowledgeArray = json["knowledge_points"].toArray();

    qDebug() << "收到保存知识库请求 - 用户名:" << username;

    // 获取用户信息
    User user = DatabaseManager::getInstance().getUserByUsername(username);
    if (user.user_id == 0) {
        qDebug() << "用户不存在:" << username;
        sendKnowledgeResponse(socket, "error", "用户不存在");
        return;
    }

    // 清空原有知识点
    DatabaseManager::getInstance().clearUserKnowledge(user.user_id);

    // 保存学习目标
    if (!learning_goal.isEmpty()) {
        DatabaseManager::getInstance().updateUserLearningGoal(user.user_id, learning_goal);
    }

    // 添加新知识点
    for (const QJsonValue &value : knowledgeArray) {
        QString point = value.toString();
        if (!point.isEmpty()) {
            DatabaseManager::getInstance().addKnowledgePoint(user.user_id, point);
        }
    }

    // 记录日志
    DatabaseManager::getInstance().logUserAction(
        user.user_id,
        "save_knowledge",
        socket->peerAddress().toString(),
        "User saved " + QString::number(knowledgeArray.size()) + " knowledge points"
    );

    qDebug() << "知识库保存成功 - 用户名:" << username << "知识点数:" << knowledgeArray.size();

    sendKnowledgeResponse(socket, "success", "知识库保存成功");
}

void MainWindow::handleGetKnowledgeRequest(const QJsonObject &json, QTcpSocket *socket)
{
    QString username = json["username"].toString();

    qDebug() << "收到获取知识库请求 - 用户名:" << username;

    // 获取用户信息
    User user = DatabaseManager::getInstance().getUserByUsername(username);
    if (user.user_id == 0) {
        qDebug() << "用户不存在:" << username;
        sendKnowledgeResponse(socket, "error", "用户不存在");
        return;
    }

    // 获取知识点列表
    QStringList knowledgePoints = DatabaseManager::getInstance().getUserKnowledgePoints(user.user_id);

    qDebug() << "获取知识库成功 - 用户名:" << username << "知识点数:" << knowledgePoints.size();

    sendKnowledgeResponse(socket, "success", "获取成功", knowledgePoints);
}

void MainWindow::sendKnowledgeResponse(QTcpSocket *socket, const QString &status,
                                      const QString &message, const QStringList &knowledgeList)
{
    QJsonObject json;
    json["type"] = "KnowledgeResponse";
    json["status"] = status;
    json["message"] = message;

    if (!knowledgeList.isEmpty()) {
        QJsonArray knowledgeArray;
        for (const QString &point : knowledgeList) {
            knowledgeArray.append(point);
        }
        json["knowledge_points"] = knowledgeArray;
    }

    QJsonDocument doc(json);
    QByteArray data = doc.toJson();

    qDebug() << "发送知识库响应:" << status;

    socket->write(data);
    socket->flush();
    socket->waitForBytesWritten(1000);
    socket->disconnectFromHost();
}
