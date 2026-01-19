#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "config.h"
#include "aichatmanager.h"
#include "learningpathmanager.h"
#include "resourcemanager.h"

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
    else if (type == AIChatType) {
        handleAIChatRequest(jsonobj, tmpsocket);
    }
    else if (type == GetAIChatHistoryType) {
        handleGetAIChatHistoryRequest(jsonobj, tmpsocket);
    }
    else if (type == GetSessionListType) {
        handleGetSessionListRequest(jsonobj, tmpsocket);
    }
    else if (type == DeleteSessionType) {
        handleDeleteSessionRequest(jsonobj, tmpsocket);
    }
    else if (type == GenerateLearningPathType) {
        handleGeneratePathRequest(jsonobj, tmpsocket);
    }
    else if (type == GetLearningPathListType) {
        handleGetPathListRequest(jsonobj, tmpsocket);
    }
    else if (type == GetLearningPathDetailType) {
        handleGetPathDetailRequest(jsonobj, tmpsocket);
    }
    else if (type == DeleteLearningPathType) {
        handleDeletePathRequest(jsonobj, tmpsocket);
    }
    else if (type == UpdatePathProgressType) {
        handleUpdatePathProgressRequest(jsonobj, tmpsocket);
    }
    else if (type == UpdateStepProgressType) {
        handleUpdateStepProgressRequest(jsonobj, tmpsocket);
    }
    else if (type == GenerateResourcesType) {
        handleGenerateResourcesRequest(jsonobj, tmpsocket);
    }
    else if (type == GetResourcesType) {
        handleGetResourcesRequest(jsonobj, tmpsocket);
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

    // 差异化更新：获取数据库中现有的知识点
    QSet<QString> dbPoints;
    QSqlQuery selectQuery(db);
    selectQuery.prepare("SELECT knowledge_point FROM user_knowledge WHERE user_id = :user_id");
    selectQuery.bindValue(":user_id", user.user_id);
    if (selectQuery.exec()) {
        while (selectQuery.next()) {
            dbPoints.insert(selectQuery.value(0).toString());
        }
        qDebug() << "数据库中现有知识点数:" << dbPoints.size();
    } else {
        qDebug() << "查询现有知识点失败:" << selectQuery.lastError().text();
        hasError = true;
    }

    // 构建客户端发送的知识点集合
    QSet<QString> clientPoints;
    for (const QJsonValue &value : knowledgeArray) {
        QString point = value.toString().trimmed();
        if (!point.isEmpty()) {
            clientPoints.insert(point);
        }
    }
    qDebug() << "客户端发送的知识点数:" << clientPoints.size();

    // 删除数据库有但客户端没有的（被用户删除的）
    QSet<QString> toDelete = dbPoints - clientPoints;
    for (const QString &point : toDelete) {
        QSqlQuery delQuery(db);
        delQuery.prepare("DELETE FROM user_knowledge WHERE user_id = :user_id AND knowledge_point = :point");
        delQuery.bindValue(":user_id", user.user_id);
        delQuery.bindValue(":point", point);
        if (!delQuery.exec()) {
            qDebug() << "知识点删除失败:" << point << delQuery.lastError().text();
            hasError = true;
        } else {
            qDebug() << "知识点已删除:" << point;
        }
    }

    // 插入客户端有但数据库没有的（新增的）
    QSet<QString> toInsert = clientPoints - dbPoints;
    for (const QString &point : toInsert) {
        QSqlQuery insQuery(db);
        insQuery.prepare("INSERT INTO user_knowledge (user_id, knowledge_point) VALUES (:user_id, :point)");
        insQuery.bindValue(":user_id", user.user_id);
        insQuery.bindValue(":point", point);
        if (!insQuery.exec()) {
            qDebug() << "知识点添加失败:" << point << insQuery.lastError().text();
            hasError = true;
        } else {
            qDebug() << "知识点已添加:" << point;
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

// ========== AI对话相关 ==========

void MainWindow::handleAIChatRequest(const QJsonObject &json, QTcpSocket *socket)
{
    QString username = json["username"].toString();
    QString message = json["message"].toString();
    QString sessionId = json["session_id"].toString();  // 可选，空则新建会话

    qDebug() << "=== handleAIChatRequest 开始 ===";
    qDebug() << "用户:" << username;
    qDebug() << "消息:" << message.left(100);
    qDebug() << "会话ID:" << (sessionId.isEmpty() ? "(新建)" : sessionId);

    // 验证用户存在
    User user = DatabaseManager::getInstance().getUserByUsername(username);
    if (user.user_id == 0) {
        qDebug() << "用户不存在:" << username;
        sendAIChatResponse(socket, "error", "用户不存在");
        return;
    }

    // 调用AI聊天管理器（同步版本，简化实现）
    AIChatResponse response = AIChatManager::getInstance().sendChatRequestSync(username, message, sessionId);

    if (response.success) {
        qDebug() << "AI响应成功，内容:" << response.content.left(100);
        sendAIChatResponse(socket, "success", "OK", response.content, response.sessionId);
    } else {
        qDebug() << "AI响应失败:" << response.error;
        sendAIChatResponse(socket, "error", response.error, "", response.sessionId);
    }

    qDebug() << "=== handleAIChatRequest 结束 ===";
}

void MainWindow::handleGetAIChatHistoryRequest(const QJsonObject &json, QTcpSocket *socket)
{
    QString username = json["username"].toString();
    QString sessionId = json["session_id"].toString();  // 可选，空则获取所有会话
    int limit = json["limit"].toInt(50);  // 默认50条

    qDebug() << "收到获取AI对话历史请求 - 用户名:" << username << "会话ID:" << sessionId;

    // 获取用户信息
    User user = DatabaseManager::getInstance().getUserByUsername(username);
    if (user.user_id == 0) {
        qDebug() << "用户不存在:" << username;
        sendAIChatResponse(socket, "error", "用户不存在");
        return;
    }

    // 获取对话历史
    QList<QJsonObject> history = DatabaseManager::getInstance().getAIChatHistory(user.user_id, sessionId, limit);

    // 构建响应
    QJsonObject responseJson;
    responseJson["type"] = "AIChatHistoryResponse";
    responseJson["status"] = "success";
    responseJson["message"] = "获取成功";

    QJsonArray historyArray;
    for (const QJsonObject &msg : history) {
        historyArray.append(msg);
    }
    responseJson["history"] = historyArray;

    // 获取最后一次会话ID
    QString lastSessionId = DatabaseManager::getInstance().getLastSessionId(user.user_id);
    responseJson["last_session_id"] = lastSessionId;

    QJsonDocument doc(responseJson);
    QByteArray data = doc.toJson();

    socket->write(data);
    socket->flush();

    qDebug() << "获取AI对话历史成功，共" << history.size() << "条记录";
}

void MainWindow::sendAIChatResponse(QTcpSocket *socket, const QString &status,
                                    const QString &message, const QString &content,
                                    const QString &sessionId)
{
    qDebug() << "=== sendAIChatResponse 开始 ===";
    qDebug() << "socket状态:" << socket->state();

    QJsonObject json;
    json["type"] = "AIChatResponse";
    json["status"] = status;
    json["message"] = message;
    json["content"] = content;
    json["session_id"] = sessionId;

    QJsonDocument doc(json);
    QByteArray data = doc.toJson();

    qDebug() << "AI响应数据长度:" << data.length();

    qint64 bytesWritten = socket->write(data);
    socket->flush();

    qDebug() << "已发送AI响应:" << bytesWritten << "字节, status:" << status;
}

// ========== 会话管理相关 ==========

void MainWindow::handleGetSessionListRequest(const QJsonObject &json, QTcpSocket *socket)
{
    QString username = json["username"].toString();

    qDebug() << "收到获取会话列表请求 - 用户名:" << username;

    // 获取用户信息
    User user = DatabaseManager::getInstance().getUserByUsername(username);
    if (user.user_id == 0) {
        qDebug() << "用户不存在:" << username;
        QJsonObject responseJson;
        responseJson["type"] = "SessionListResponse";
        responseJson["status"] = "error";
        responseJson["message"] = "用户不存在";
        QJsonDocument doc(responseJson);
        socket->write(doc.toJson());
        socket->flush();
        return;
    }

    // 获取会话列表
    QList<QJsonObject> sessions = DatabaseManager::getInstance().getSessionList(user.user_id);

    // 构建响应
    QJsonObject responseJson;
    responseJson["type"] = "SessionListResponse";
    responseJson["status"] = "success";
    responseJson["message"] = "获取成功";

    QJsonArray sessionArray;
    for (const QJsonObject &session : sessions) {
        sessionArray.append(session);
    }
    responseJson["sessions"] = sessionArray;

    QJsonDocument doc(responseJson);
    QByteArray data = doc.toJson();

    socket->write(data);
    socket->flush();

    qDebug() << "获取会话列表成功，共" << sessions.size() << "个会话";
}

void MainWindow::handleDeleteSessionRequest(const QJsonObject &json, QTcpSocket *socket)
{
    QString username = json["username"].toString();
    QString sessionId = json["session_id"].toString();

    qDebug() << "收到删除会话请求 - 用户名:" << username << "会话ID:" << sessionId;

    // 获取用户信息
    User user = DatabaseManager::getInstance().getUserByUsername(username);
    if (user.user_id == 0) {
        qDebug() << "用户不存在:" << username;
        QJsonObject responseJson;
        responseJson["type"] = "DeleteSessionResponse";
        responseJson["status"] = "error";
        responseJson["message"] = "用户不存在";
        QJsonDocument doc(responseJson);
        socket->write(doc.toJson());
        socket->flush();
        return;
    }

    // 删除会话
    bool success = DatabaseManager::getInstance().deleteSession(user.user_id, sessionId);

    // 构建响应
    QJsonObject responseJson;
    responseJson["type"] = "DeleteSessionResponse";
    if (success) {
        responseJson["status"] = "success";
        responseJson["message"] = "会话已删除";
        qDebug() << "删除会话成功";
    } else {
        responseJson["status"] = "error";
        responseJson["message"] = "删除会话失败";
        qDebug() << "删除会话失败";
    }

    QJsonDocument doc(responseJson);
    socket->write(doc.toJson());
    socket->flush();
}

// ========== 学习路径相关 ==========

void MainWindow::handleGeneratePathRequest(const QJsonObject &json, QTcpSocket *socket)
{
    QString username = json["username"].toString();
    QString path_name = json["path_name"].toString();
    QString learning_goal = json["learning_goal"].toString();

    QJsonArray knowledgeArray = json["current_knowledge"].toArray();
    QStringList current_knowledge;
    for (const QJsonValue &value : knowledgeArray) {
        current_knowledge.append(value.toString());
    }

    QString time_preference = json.value("time_preference").toString("3个月");
    QString difficulty = json.value("difficulty").toString("intermediate");

    qDebug() << "=== handleGeneratePathRequest 开始 ===";
    qDebug() << "用户:" << username;
    qDebug() << "路径名称:" << path_name;
    qDebug() << "学习目标:" << learning_goal;

    // 获取用户信息
    User user = DatabaseManager::getInstance().getUserByUsername(username);
    if (user.user_id == 0) {
        qDebug() << "用户不存在:" << username;
        sendPathResponse(socket, "error", "用户不存在");
        return;
    }

    // 调用LearningPathManager生成路径
    GeneratePathResponse response = LearningPathManager::getInstance().generatePath(
        user.user_id, path_name, learning_goal, current_knowledge, time_preference, difficulty
    );

    if (response.success) {
        qDebug() << "学习路径生成成功，path_id:" << response.path_id;

        // 构建响应
        QJsonObject data;
        data["path_id"] = response.path_id;
        data["path_name"] = path_name;
        data["learning_goal"] = learning_goal;
        data["path_data"] = response.path_data;

        sendPathResponse(socket, "success", "学习路径生成成功", data);
    } else {
        qDebug() << "学习路径生成失败:" << response.error;
        sendPathResponse(socket, "error", response.error);
    }
}

void MainWindow::handleGetPathListRequest(const QJsonObject &json, QTcpSocket *socket)
{
    QString username = json["username"].toString();

    qDebug() << "收到获取路径列表请求 - 用户名:" << username;

    // 获取用户信息
    User user = DatabaseManager::getInstance().getUserByUsername(username);
    if (user.user_id == 0) {
        qDebug() << "用户不存在:" << username;
        sendPathResponse(socket, "error", "用户不存在");
        return;
    }

    // 获取路径列表
    QList<LearningPath> paths = LearningPathManager::getInstance().getUserPaths(user.user_id);

    // 构建响应
    QJsonObject responseJson;
    responseJson["type"] = "PathListResponse";
    responseJson["status"] = "success";
    responseJson["message"] = "获取成功";

    QJsonArray pathArray;
    for (const LearningPath &path : paths) {
        QJsonObject pathObj;
        pathObj["path_id"] = path.path_id;
        pathObj["path_name"] = path.path_name;
        pathObj["learning_goal"] = path.learning_goal;
        pathObj["status"] = path.status;
        pathObj["progress"] = path.progress;
        pathObj["created_at"] = path.created_at;
        pathArray.append(pathObj);
    }
    responseJson["paths"] = pathArray;

    QJsonDocument doc(responseJson);
    socket->write(doc.toJson());
    socket->flush();

    qDebug() << "获取路径列表成功，共" << paths.size() << "条路径";
}

void MainWindow::handleGetPathDetailRequest(const QJsonObject &json, QTcpSocket *socket)
{
    QString username = json["username"].toString();
    int path_id = json["path_id"].toInt();

    qDebug() << "收到获取路径详情请求 - 用户名:" << username << "path_id:" << path_id;

    // 获取用户信息
    User user = DatabaseManager::getInstance().getUserByUsername(username);
    if (user.user_id == 0) {
        qDebug() << "用户不存在:" << username;
        sendPathResponse(socket, "error", "用户不存在");
        return;
    }

    // 获取路径详情
    LearningPath path = LearningPathManager::getInstance().getPathDetail(path_id, user.user_id);

    if (path.path_id == 0) {
        qDebug() << "路径不存在或无权访问";
        sendPathResponse(socket, "error", "路径不存在或无权访问");
        return;
    }

    // 解析path_data
    QJsonDocument pathDoc = QJsonDocument::fromJson(path.path_data.toUtf8());
    QJsonObject pathData = pathDoc.object();

    // 验证path_data是否有效
    if (pathData.isEmpty() || !pathData.contains("stages")) {
        qDebug() << "错误: path_data为空或缺少stages字段 - path_id:" << path_id;
        sendPathResponse(socket, "error", "路径数据不完整，请重新生成路径");
        return;
    }

    QJsonArray stagesArray = pathData["stages"].toArray();
    if (stagesArray.isEmpty()) {
        qDebug() << "警告: path_data中stages数组为空 - path_id:" << path_id;
    }

    // 构建响应
    QJsonObject responseJson;
    responseJson["type"] = "PathDetailResponse";
    responseJson["status"] = "success";
    responseJson["message"] = "获取成功";
    responseJson["path_id"] = path.path_id;
    responseJson["path_name"] = path.path_name;
    responseJson["learning_goal"] = path.learning_goal;
    responseJson["path_status"] = path.status;  // 修复: 使用path_status避免覆盖status字段
    responseJson["progress"] = path.progress;
    responseJson["created_at"] = path.created_at;
    responseJson["updated_at"] = path.updated_at;
    responseJson["path_data"] = pathData;

    QJsonDocument doc(responseJson);
    socket->write(doc.toJson());
    socket->flush();

    qDebug() << "获取路径详情成功";
}

void MainWindow::handleDeletePathRequest(const QJsonObject &json, QTcpSocket *socket)
{
    QString username = json["username"].toString();
    int path_id = json["path_id"].toInt();

    qDebug() << "收到删除路径请求 - 用户名:" << username << "path_id:" << path_id;

    // 获取用户信息
    User user = DatabaseManager::getInstance().getUserByUsername(username);
    if (user.user_id == 0) {
        qDebug() << "用户不存在:" << username;
        sendPathResponse(socket, "error", "用户不存在");
        return;
    }

    // 删除路径
    bool success = LearningPathManager::getInstance().deletePath(path_id, user.user_id);

    // 构建响应
    QJsonObject responseJson;
    responseJson["type"] = "DeletePathResponse";
    if (success) {
        responseJson["status"] = "success";
        responseJson["message"] = "路径已删除";
        qDebug() << "删除路径成功";
    } else {
        responseJson["status"] = "error";
        responseJson["message"] = "删除路径失败";
        qDebug() << "删除路径失败";
    }

    QJsonDocument doc(responseJson);
    socket->write(doc.toJson());
    socket->flush();
}

void MainWindow::handleUpdatePathProgressRequest(const QJsonObject &json, QTcpSocket *socket)
{
    QString username = json["username"].toString();
    int path_id = json["path_id"].toInt();
    int stage_order = json["stage_order"].toInt();
    bool completed = json["completed"].toBool();
    int completed_count = json["completed_count"].toInt();
    int total_count = json["total_count"].toInt();

    qDebug() << "收到更新路径进度请求 - 用户名:" << username << "path_id:" << path_id << "stage_order:" << stage_order << "completed:" << completed;

    // 获取用户信息
    User user = DatabaseManager::getInstance().getUserByUsername(username);
    if (user.user_id == 0) {
        qDebug() << "用户不存在:" << username;
        sendPathResponse(socket, "error", "用户不存在");
        return;
    }

    // 设置阶段完成状态
    bool success = LearningPathManager::getInstance().setStageCompleted(path_id, stage_order, completed);

    double progress = 0.0;
    int status = 0;
    int completedSteps = 0;
    int totalSteps = 0;

    if (success) {
        // 同步步骤状态到阶段完成状态（仅当阶段标记为完成时）
        if (completed) {
            LearningPathManager::getInstance().syncStepsToStageCompletion(path_id, stage_order, completed);
        }

        // 计算并更新路径进度（使用步骤计数而非阶段计数）
        QPair<int, int> stepProgress = LearningPathManager::getInstance().calculateStepProgress(path_id);
        completedSteps = stepProgress.first;
        totalSteps = stepProgress.second;
        progress = totalSteps > 0 ? (double)completedSteps / totalSteps : 0.0;

        LearningPathManager::getInstance().updatePathProgress(path_id, user.user_id, progress);

        // 根据进度更新状态
        status = 0;  // 未开始
        if (progress > 0) status = 1;  // 进行中
        if (progress >= 1.0) status = 2;  // 已完成
        LearningPathManager::getInstance().updatePathStatus(path_id, user.user_id, status);

        qDebug() << "更新路径进度成功 - progress:" << progress
                 << "completedSteps:" << completedSteps << "totalSteps:" << totalSteps
                 << "status:" << status;
    }

    // 构建响应 - 包含更新后的进度和状态信息
    QJsonObject responseJson;
    responseJson["type"] = "UpdatePathProgressResponse";
    responseJson["status"] = success ? "success" : "error";
    responseJson["message"] = success ? "进度更新成功" : "进度更新失败";

    if (success) {
        // 添加更新后的进度信息，供客户端刷新UI使用
        responseJson["progress"] = progress;
        responseJson["path_status"] = status;
        responseJson["completed_steps"] = completedSteps;
        responseJson["total_steps"] = totalSteps;
    }

    QJsonDocument doc(responseJson);
    socket->write(doc.toJson());
    socket->flush();
}

void MainWindow::handleUpdateStepProgressRequest(const QJsonObject &json, QTcpSocket *socket)
{
    QString username = json["username"].toString();
    int path_id = json["path_id"].toInt();
    int stage_order = json["stage_order"].toInt();
    int step_order = json["step_order"].toInt();
    bool completed = json["completed"].toBool();

    qDebug() << "收到更新步骤进度请求 - 用户名:" << username << "path_id:" << path_id
             << "stage_order:" << stage_order << "step_order:" << step_order << "completed:" << completed;

    // 获取用户信息
    User user = DatabaseManager::getInstance().getUserByUsername(username);
    if (user.user_id == 0) {
        qDebug() << "用户不存在:" << username;
        sendPathResponse(socket, "error", "用户不存在");
        return;
    }

    // 更新步骤完成状态
    bool success = LearningPathManager::getInstance().updateStepProgress(path_id, stage_order, step_order, completed);

    if (success) {
        // 获取更新后的进度信息
        QPair<int, int> progress = LearningPathManager::getInstance().calculateStepProgress(path_id);
        int completedSteps = progress.first;
        int totalSteps = progress.second;
        double progressValue = totalSteps > 0 ? (double)completedSteps / totalSteps : 0.0;
        int status = 0;
        if (progressValue > 0) status = 1;
        if (progressValue >= 1.0) status = 2;

        // 构建响应
        QJsonObject responseJson;
        responseJson["type"] = "UpdateStepProgressResponse";
        responseJson["status"] = "success";
        responseJson["message"] = "步骤进度更新成功";
        responseJson["progress"] = progressValue;
        responseJson["path_status"] = status;
        responseJson["completed_steps"] = completedSteps;
        responseJson["total_steps"] = totalSteps;

        QJsonDocument doc(responseJson);
        socket->write(doc.toJson());
        socket->flush();

        qDebug() << "步骤进度更新成功 - progress:" << progressValue << "status:" << status;
    } else {
        sendPathResponse(socket, "error", "步骤进度更新失败");
    }
}

void MainWindow::sendPathResponse(QTcpSocket *socket, const QString &status,
                                   const QString &message, const QJsonObject &data)
{
    qDebug() << "=== sendPathResponse 开始 ===";
    qDebug() << "socket状态:" << socket->state();

    QJsonObject json;
    json["type"] = "PathResponse";
    json["status"] = status;
    json["message"] = message;

    if (!data.isEmpty()) {
        for (const QString &key : data.keys()) {
            json[key] = data[key];
        }
    }

    QJsonDocument doc(json);
    QByteArray responseData = doc.toJson();

    qDebug() << "路径响应数据长度:" << responseData.length();

    qint64 bytesWritten = socket->write(responseData);
    socket->flush();

    qDebug() << "已发送路径响应:" << bytesWritten << "字节, status:" << status;
}

// ========== 学习资源相关 ==========

void MainWindow::handleGenerateResourcesRequest(const QJsonObject &json, QTcpSocket *socket)
{
    qDebug() << "=== 收到生成资源推荐请求 ===";

    int path_id = json["path_id"].toInt();
    QString path_name = json["path_name"].toString();
    QJsonArray stages = json["stages"].toArray();

    qDebug() << "路径ID:" << path_id;
    qDebug() << "路径名称:" << path_name;
    qDebug() << "阶段数量:" << stages.size();

    // 如果stages为空，从数据库获取路径详情
    if (stages.isEmpty() && path_id > 0) {
        qDebug() << "stages为空，从数据库获取路径详情";
        QSqlQuery query(_db);
        query.prepare("SELECT path_name, path_data FROM learning_paths WHERE path_id = :path_id");
        query.bindValue(":path_id", path_id);

        if (query.exec() && query.next()) {
            path_name = query.value(0).toString();
            QString pathDataStr = query.value(1).toString();

            QJsonDocument pathDoc = QJsonDocument::fromJson(pathDataStr.toUtf8());
            if (!pathDoc.isNull() && pathDoc.isObject()) {
                QJsonObject pathObj = pathDoc.object();
                stages = pathObj["stages"].toArray();
                qDebug() << "从数据库获取到阶段数量:" << stages.size();
            }
        }
    }

    if (path_id <= 0 || stages.isEmpty()) {
        sendResourcesResponse(socket, "error", "参数错误：路径ID或阶段数据为空，请确认学习路径已正确生成");
        return;
    }

    // 调用ResourceManager生成资源推荐
    ResourceGenerationResponse response = ResourceManager::getInstance().generateAndSaveResources(
        path_id, path_name, stages
    );

    QJsonArray resourcesArray;
    for (const ResourceInfo &info : response.resources) {
        QJsonObject resourceObj;
        resourceObj["id"] = info.id;
        resourceObj["path_id"] = info.path_id;
        resourceObj["stage_order"] = info.stage_order;
        resourceObj["title"] = info.title;
        resourceObj["url"] = info.url;
        resourceObj["source"] = info.source;
        resourceObj["description"] = info.description;
        resourceObj["difficulty"] = info.difficulty;
        resourcesArray.append(resourceObj);
    }

    sendResourcesResponse(socket,
                          response.success ? "success" : "error",
                          response.message,
                          resourcesArray);
}

void MainWindow::handleGetResourcesRequest(const QJsonObject &json, QTcpSocket *socket)
{
    qDebug() << "=== 收到获取资源列表请求 ===";

    int path_id = json["path_id"].toInt();
    int stage_order = json["stage_order"].toInt(-1);  // 可选，-1表示获取全部

    qDebug() << "路径ID:" << path_id;
    qDebug() << "阶段序号:" << (stage_order >= 0 ? QString::number(stage_order) : "全部");

    if (path_id <= 0) {
        sendResourcesResponse(socket, "error", "参数错误：路径ID无效");
        return;
    }

    QList<ResourceInfo> resources;
    if (stage_order >= 0) {
        resources = ResourceManager::getInstance().getStageResources(path_id, stage_order);
    } else {
        resources = ResourceManager::getInstance().getPathResources(path_id);
    }

    QJsonArray resourcesArray;
    for (const ResourceInfo &info : resources) {
        QJsonObject resourceObj;
        resourceObj["id"] = info.id;
        resourceObj["path_id"] = info.path_id;
        resourceObj["stage_order"] = info.stage_order;
        resourceObj["title"] = info.title;
        resourceObj["url"] = info.url;
        resourceObj["source"] = info.source;
        resourceObj["description"] = info.description;
        resourceObj["difficulty"] = info.difficulty;
        resourcesArray.append(resourceObj);
    }

    sendResourcesResponse(socket, "success", QString("获取到%1条资源").arg(resources.size()), resourcesArray);
}

void MainWindow::sendResourcesResponse(QTcpSocket *socket, const QString &status,
                                       const QString &message, const QJsonArray &resources)
{
    qDebug() << "=== sendResourcesResponse 开始 ===";

    QJsonObject json;
    json["type"] = "ResourcesResponse";
    json["status"] = status;
    json["message"] = message;

    if (!resources.isEmpty()) {
        json["resources"] = resources;
    }

    QJsonDocument doc(json);
    QByteArray responseData = doc.toJson();

    qDebug() << "资源响应数据长度:" << responseData.length();

    qint64 bytesWritten = socket->write(responseData);
    socket->flush();

    qDebug() << "已发送资源响应:" << bytesWritten << "字节, status:" << status;
}
