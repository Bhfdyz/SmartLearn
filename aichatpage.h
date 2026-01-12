#ifndef AICHATPAGE_H
#define AICHATPAGE_H

#include <QWidget>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>

// 单条消息结构
struct ChatMessage {
    QString role;      // "user" 或 "assistant"
    QString content;
    QString timestamp;
};

class AIChatPage : public QWidget
{
    Q_OBJECT

public:
    explicit AIChatPage(const QString &username, QWidget *parent = nullptr);
    void loadChatHistory();  // 加载对话历史
    void clearChat();        // 清空当前对话（新建会话）

signals:
    void messageSent();      // 消息发送信号

private slots:
    void onSendClicked();    // 发送按钮点击
    void onInputReturnPressed();  // 输入框回车

private:
    QString _username;
    QString _currentSessionId;  // 当前会话ID

    // UI组件
    QListWidget *_chatListWidget;     // 对话列表
    QLineEdit *_inputLineEdit;        // 输入框
    QPushButton *_sendButton;         // 发送按钮
    QPushButton *_newChatButton;      // 新建对话按钮
    QLabel *_statusLabel;             // 状态标签

    // 数据
    QList<ChatMessage> _messages;

    // 方法
    void setupUI();
    void addMessage(const ChatMessage &msg);
    void addMessageToUI(const ChatMessage &msg);
    void sendMessage(const QString &message);
    QString getCurrentTime() const;
};

#endif // AICHATPAGE_H
