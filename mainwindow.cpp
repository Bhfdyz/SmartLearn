#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "config.h"

#include <QDebug>
#include <QThread>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlQuery>
#include <QSqlError>
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
    QTcpSocket *socket = _server->nextPendingConnection();
    if (socket) {
        connect(socket, &QTcpSocket::readyRead,
            this, &MainWindow::SlotReadFromClient);
        qDebug() << "新客户端已连接";
    }
}

void MainWindow::SlotReadFromClient()
{
    QTcpSocket *tmpsocket = qobject_cast<QTcpSocket*>(sender());
    QByteArray revData = tmpsocket->readAll();

    qDebug() << "=== 服务端收到数据 ===";
    qDebug() << "原始数据:" << revData;

    QJsonDocument jsondoc = QJsonDocument::fromJson(revData);
    if (jsondoc.isNull() || !jsondoc.isObject()) {
        qDebug() << "JSON解析失败！";
        return;
    }

    QJsonObject jsonobj = jsondoc.object();
    QString type = jsonobj["type"].toString();
    qDebug() << "请求类型:" << type;

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
    else {
        qDebug() << "未知的请求类型:" << type;
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
        // 暂时移除日志记录以避免数据库锁定
        // DatabaseManager::getInstance().logUserAction(userInfo.user_id, "login", socket->peerAddress().toString());

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
                            "密码长度不足（至少6位）");
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

        // 暂时移除日志记录以避免数据库锁定
        // DatabaseManager::getInstance().logUserAction(newUserId, "register", socket->peerAddress().toString(), "User registered: " + username);

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

    qDebug() << "注册响应发送完成，保持连接";

    // 不关闭连接，让客户端保持连接以便后续登录
    // socket->disconnectFromHost();
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
    // 只检查长度，至少6位
    if (password.length() < 6) {
        return false;
    }

    return true;
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

    qDebug() << "=== handleSaveKnowledgeRequest 开始 ===";
    qDebug() << "收到保存知识库请求 - 用户名:" << username;
    qDebug() << "学习目标:" << learning_goal;
    qDebug() << "知识点数:" << knowledgeArray.size();

    // 获取用户信息
    User user = DatabaseManager::getInstance().getUserByUsername(username);
    if (user.user_id == 0) {
        qDebug() << "用户不存在:" << username;
        sendKnowledgeResponse(socket, "error", "用户不存在");
        return;
    }
    qDebug() << "找到用户，user_id:" << user.user_id;

    // 使用事务来避免数据库锁定
    QSqlDatabase &db = DatabaseManager::getInstance().getDatabase();
    db.transaction();

    // 保存学习目标（如果提供了）
    bool hasError = false;
    if (!learning_goal.isEmpty()) {
        QSqlQuery query(db);
        query.prepare("UPDATE users SET learning_goal = :goal WHERE user_id = :user_id");
        query.bindValue(":goal", learning_goal);
        query.bindValue(":user_id", user.user_id);
        if (!query.exec()) {
            qDebug() << "学习目标更新失败:" << query.lastError().text();
            hasError = true;
        } else {
            qDebug() << "学习目标已更新";
        }
    }

    // 追加新知识点（不删除已存在的）
    for (const QJsonValue &value : knowledgeArray) {
        QString point = value.toString();
        if (!point.isEmpty()) {
            QSqlQuery checkQuery(db);
            checkQuery.prepare("SELECT COUNT(*) FROM user_knowledge WHERE user_id = :user_id AND knowledge_point = :point");
            checkQuery.bindValue(":user_id", user.user_id);
            checkQuery.bindValue(":point", point);

            if (checkQuery.exec() && checkQuery.next() && checkQuery.value(0).toInt() > 0) {
                // 已存在，跳过
                qDebug() << "知识点已存在，跳过:" << point;
                continue;
            }

            // 插入新知识点
            QSqlQuery insertQuery(db);
            insertQuery.prepare("INSERT INTO user_knowledge (user_id, knowledge_point) VALUES (:user_id, :point)");
            insertQuery.bindValue(":user_id", user.user_id);
            insertQuery.bindValue(":point", point);
            if (!insertQuery.exec()) {
                qDebug() << "知识点添加失败:" << point << insertQuery.lastError().text();
                hasError = true;
            } else {
                qDebug() << "知识点已添加:" << point;
            }
        }
    }

    if (hasError) {
        db.rollback();
        qDebug() << "事务回滚";
        sendKnowledgeResponse(socket, "error", "保存失败");
    } else {
        db.commit();
        qDebug() << "事务提交成功";
        qDebug() << "知识库保存成功 - 用户名:" << username << "知识点数:" << knowledgeArray.size();
        sendKnowledgeResponse(socket, "success", "知识库保存成功");
    }

    qDebug() << "=== handleSaveKnowledgeRequest 结束 ===";
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

    // 获取学习目标
    QString learningGoal = user.learning_goal;

    qDebug() << "获取知识库成功 - 用户名:" << username << "知识点数:" << knowledgePoints.size() << "学习目标:" << learningGoal;

    sendKnowledgeResponse(socket, "success", "获取成功", knowledgePoints, learningGoal);
}

void MainWindow::sendKnowledgeResponse(QTcpSocket *socket, const QString &status,
                                      const QString &message, const QStringList &knowledgeList,
                                      const QString &learningGoal)
{
    qDebug() << "=== sendKnowledgeResponse 开始 ===";
    qDebug() << "socket状态:" << socket->state();

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

    // 添加学习目标字段
    json["learning_goal"] = learningGoal;

    QJsonDocument doc(json);
    QByteArray data = doc.toJson();

    qDebug() << "响应数据:" << data;
    qDebug() << "响应长度:" << data.length();

    // 写入数据
    qint64 bytesWritten = socket->write(data);
    socket->flush();

    qDebug() << "已发送知识库响应:" << bytesWritten << "字节, status:" << status;

    // 不要关闭连接，让客户端保持连接以便后续通信
    // 客户端收到响应后会自动关闭对话框
}
