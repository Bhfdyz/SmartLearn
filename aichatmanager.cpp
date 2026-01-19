#include "aichatmanager.h"
#include "databasemanager.h"
#include "config.h"

#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkReply>
#include <QDebug>
#include <QSslConfiguration>
#include <QEventLoop>
#include <QTimer>
#include <QProcess>
#include <QTemporaryFile>

AIChatManager &AIChatManager::getInstance()
{
    static AIChatManager manager;
    return manager;
}

AIChatManager::AIChatManager(QObject *parent)
    : QObject(parent)
    , _manager(new QNetworkAccessManager(this))
    , _apiKey("sk-b10a2912a78c4a47bb4908039e64f968")  // TODO: 移到配置文件
{
    // 使用 curl 绕过 Qt SSL 问题
    qDebug() << "AIChatManager 初始化完成，将使用 curl 进行 HTTPS 请求";
}

QString AIChatManager::buildSystemPrompt(const QString &username)
{
    // 获取用户信息
    User user = DatabaseManager::getInstance().getUserByUsername(username);
    if (user.user_id == 0) {
        return "你是一个专业的学习助手，帮助学生规划学习路径。";
    }

    // 获取用户的知识点
    QStringList knowledgePoints = DatabaseManager::getInstance().getUserKnowledgePoints(user.user_id);

    // 构建系统提示词
    QString prompt = "你是 SmartLearn 智能学习助手，负责为学生提供个性化的学习建议。\n\n";

    if (!user.learning_goal.isEmpty()) {
        prompt += "【学生的学习目标】\n" + user.learning_goal + "\n\n";
    }

    if (!knowledgePoints.isEmpty()) {
        prompt += "【学生已掌握的知识点】\n";
        for (const QString &point : knowledgePoints) {
            prompt += "• " + point + "\n";
        }
        prompt += "\n";
    }

    prompt += "【你的任务】\n";
    prompt += "1. 根据学生的知识掌握情况和学习目标，给出针对性的学习建议\n";
    prompt += "2. 推荐下一步应该学习的知识点\n";
    prompt += "3. 回答学生的疑问，帮助他们理解知识点\n";
    prompt += "4. 保持友好、专业的语气\n";

    return prompt;
}

QString AIChatManager::sendChatRequest(const QString &username, const QString &userMessage, const QString &sessionId)
{
    // 获取用户信息
    User user = DatabaseManager::getInstance().getUserByUsername(username);
    if (user.user_id == 0) {
        emit requestError("", "用户不存在");
        return "";
    }

    // 确定会话ID
    QString currentSessionId = sessionId;
    if (currentSessionId.isEmpty()) {
        currentSessionId = DatabaseManager::getInstance().generateSessionId();
    }

    // 保存用户消息
    DatabaseManager::getInstance().saveAIChatMessage(user.user_id, "user", userMessage, currentSessionId);

    // 获取对话历史（用于上下文）
    QList<QJsonObject> history = DatabaseManager::getInstance().getAIChatHistory(user.user_id, currentSessionId, 20);

    // 构建请求消息数组
    QJsonArray messages;
    messages.append(QJsonObject{
        {"role", "system"},
        {"content", buildSystemPrompt(username)}
    });

    for (const QJsonObject &msg : history) {
        messages.append(QJsonObject{
            {"role", msg["role"].toString()},
            {"content", msg["content"].toString()}
        });
    }

    // 构建请求体
    QJsonObject requestBody;
    requestBody["model"] = getModel();
    requestBody["messages"] = messages;
    requestBody["stream"] = false;  // 暂时使用非流式，简化实现
    requestBody["temperature"] = 1.0;
    requestBody["max_tokens"] = 2048;

    QJsonDocument doc(requestBody);
    QByteArray jsonData = doc.toJson();

    qDebug() << "=== AI聊天请求 ===";
    qDebug() << "用户:" << username;
    qDebug() << "会话ID:" << currentSessionId;
    qDebug() << "请求体:" << jsonData.left(200) + "...";

    // 发送HTTP请求
    QNetworkRequest request{QUrl(getApiEndpoint())};
    request.setRawHeader("Content-Type", "application/json");
    request.setRawHeader("Authorization", "Bearer " + _apiKey.toUtf8());
    request.setRawHeader("Accept", "application/json");

    QNetworkReply *reply = _manager->post(request, jsonData);

    // 关联回复与会话ID
    _replySessionMap[reply] = currentSessionId;

    // 连接信号
    connect(reply, &QNetworkReply::finished, this, &AIChatManager::onFinished);
    connect(reply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::error),
            this, &AIChatManager::onError);

    return currentSessionId;
}

AIChatResponse AIChatManager::sendChatRequestSync(const QString &username, const QString &userMessage, const QString &sessionId)
{
    // 这个同步版本使用 curl 来绕过 Qt SSL 问题
    AIChatResponse response;
    response.success = false;

    User user = DatabaseManager::getInstance().getUserByUsername(username);
    if (user.user_id == 0) {
        response.error = "用户不存在";
        return response;
    }

    QString currentSessionId = sessionId;
    if (currentSessionId.isEmpty()) {
        currentSessionId = DatabaseManager::getInstance().generateSessionId();
    }
    response.sessionId = currentSessionId;

    // 保存用户消息
    DatabaseManager::getInstance().saveAIChatMessage(user.user_id, "user", userMessage, currentSessionId);

    // 获取对话历史
    QList<QJsonObject> history = DatabaseManager::getInstance().getAIChatHistory(user.user_id, currentSessionId, 20);

    // 构建请求
    QJsonArray messages;
    messages.append(QJsonObject{
        {"role", "system"},
        {"content", buildSystemPrompt(username)}
    });

    for (const QJsonObject &msg : history) {
        messages.append(QJsonObject{
            {"role", msg["role"].toString()},
            {"content", msg["content"].toString()}
        });
    }

    QJsonObject requestBody;
    requestBody["model"] = getModel();
    requestBody["messages"] = messages;
    requestBody["stream"] = false;
    requestBody["temperature"] = 1.0;
    requestBody["max_tokens"] = 2048;

    QJsonDocument doc(requestBody);
    QByteArray jsonData = doc.toJson();

    qDebug() << "=== AI聊天请求 ===";
    qDebug() << "使用 curl 发送请求";

    // 使用 QProcess 调用 curl
    QProcess process;

    // 创建临时文件存储请求数据
    QTemporaryFile tempFile;
    if (tempFile.open()) {
        tempFile.write(jsonData);
        tempFile.flush();
        tempFile.close();
    }

    // 构建 curl 命令
    QStringList args;
    args << "--connect-timeout" << "10";     // 连接超时 10 秒
    args << "--max-time" << "60";            // 最大总时间 60 秒（AI聊天）
    args << "-X" << "POST";
    args << "-H" << "Content-Type: application/json";
    args << "-H" << "Accept: application/json";
    args << "-H" << ("Authorization: Bearer " + _apiKey);
    args << "-d" << ("@" + tempFile.fileName());
    args << getApiEndpoint();

    qDebug() << "curl命令: curl" << args.join(" ");

    // 启动进程
    process.start("curl", args);

    // 等待完成（70秒超时 = curl max-time 60秒 + 10秒缓冲）
    if (!process.waitForFinished(70000)) {
        response.error = "请求超时";
        qDebug() << "curl请求超时";
        return response;
    }

    QByteArray responseData = process.readAllStandardOutput();
    QByteArray errorData = process.readAllStandardError();

    qDebug() << "curl退出码:" << process.exitCode();
    if (!errorData.isEmpty()) {
        qDebug() << "curl错误输出:" << errorData;
    }
    qDebug() << "AI响应数据:" << responseData.left(500);

    // 检查 curl 是否成功
    if (process.exitCode() != 0) {
        response.error = "curl请求失败: " + QString::fromUtf8(errorData);
        return response;
    }

    // 解析响应
    QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);
    if (responseDoc.isNull()) {
        response.error = "解析JSON失败，原始响应: " + QString::fromUtf8(responseData.left(200));
        qDebug() << response.error;
    } else if (!responseDoc.isObject()) {
        response.error = "响应不是JSON对象";
        qDebug() << response.error;
    } else {
        QJsonObject responseObject = responseDoc.object();

        // 检查API错误
        if (responseObject.contains("error")) {
            QJsonObject errorObj = responseObject["error"].toObject();
            response.error = "API错误: " + errorObj["message"].toString();
            qDebug() << response.error;
        } else {
            QJsonArray choices = responseObject["choices"].toArray();
            if (!choices.isEmpty()) {
                QJsonObject choice = choices[0].toObject();
                QJsonObject messageObj = choice["message"].toObject();
                QString content = messageObj["content"].toString();

                response.success = true;
                response.content = content;

                // 保存助手回复
                DatabaseManager::getInstance().saveAIChatMessage(user.user_id, "assistant", content, currentSessionId);
            } else {
                response.error = "AI返回choices为空，完整响应: " + QString::fromUtf8(responseData);
                qDebug() << response.error;
            }
        }
    }

    return response;
}

void AIChatManager::onReadyRead()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    QString sessionId = _replySessionMap.value(reply, "");
    QByteArray data = reply->readAll();

    // TODO: 处理流式响应
    qDebug() << "流式数据[" << sessionId << "]:" << data.left(100);
}

void AIChatManager::onFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    QString sessionId = _replySessionMap.value(reply, "");
    QString username = _replyBuffer.value(reply, "");  // 这里需要额外的映射来获取username

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray responseData = reply->readAll();
        qDebug() << "=== AI响应 ===";
        qDebug() << "会话ID:" << sessionId;
        qDebug() << "响应数据:" << responseData.left(500);

        QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);
        if (!responseDoc.isNull() && responseDoc.isObject()) {
            QJsonObject responseObject = responseDoc.object();
            QJsonArray choices = responseObject["choices"].toArray();
            if (!choices.isEmpty()) {
                QJsonObject choice = choices[0].toObject();
                QJsonObject messageObj = choice["message"].toObject();
                QString content = messageObj["content"].toString();

                // 通过信号发送内容
                emit requestFinished(sessionId, true, content);
            } else {
                emit requestError(sessionId, "AI返回为空");
            }
        } else {
            emit requestError(sessionId, "解析AI响应失败");
        }
    }

    _replySessionMap.remove(reply);
    _replyBuffer.remove(reply);
    reply->deleteLater();
}

void AIChatManager::onError(QNetworkReply::NetworkError code)
{
    Q_UNUSED(code)
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    QString sessionId = _replySessionMap.value(reply, "");
    QString errorMsg = "网络错误: " + reply->errorString();

    qDebug() << "AI请求错误[" << sessionId << "]:" << errorMsg;
    emit requestError(sessionId, errorMsg);

    _replySessionMap.remove(reply);
    _replyBuffer.remove(reply);
}
