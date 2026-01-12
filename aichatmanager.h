#ifndef AICHATMANAGER_H
#define AICHATMANAGER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>

// AI对话响应结构
struct AIChatResponse {
    bool success;
    QString content;
    QString error;
    QString sessionId;
};

class AIChatManager : public QObject
{
    Q_OBJECT

public:
    static AIChatManager& getInstance();

    // 发送聊天请求（非阻塞，返回sessionId）
    QString sendChatRequest(const QString &username, const QString &userMessage,
                           const QString &sessionId = "");

    // 构建系统提示词（基于用户知识库和学习目标）
    QString buildSystemPrompt(const QString &username);

    // 同步版本（用于简单场景，会阻塞）
    AIChatResponse sendChatRequestSync(const QString &username, const QString &userMessage,
                                      const QString &sessionId = "");

signals:
    // 流式数据到达信号
    void streamDataReceived(const QString &sessionId, const QString &content);
    // 请求完成信号
    void requestFinished(const QString &sessionId, bool success, const QString &fullContent);
    // 请求错误信号
    void requestError(const QString &sessionId, const QString &error);

private slots:
    void onReadyRead();
    void onFinished();
    void onError(QNetworkReply::NetworkError code);

private:
    AIChatManager(QObject *parent = nullptr);
    AIChatManager(const AIChatManager&) = delete;
    AIChatManager& operator=(const AIChatManager&) = delete;

    QNetworkAccessManager *_manager;
    QMap<QNetworkReply*, QString> _replySessionMap;  // reply -> sessionId
    QMap<QNetworkReply*, QString> _replyBuffer;      // reply -> accumulated content
    QString _apiKey;

    // API配置
    QString getApiEndpoint() const { return "https://api.deepseek.com/chat/completions"; }
    QString getModel() const { return "deepseek-chat"; }
};

#endif // AICHATMANAGER_H
