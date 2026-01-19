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
#include <QSplitter>
#include <QResizeEvent>
#include <QThread>

// 单条消息结构
struct ChatMessage {
    QString role;      // "user" 或 "assistant"
    QString content;
    QString timestamp;
};

// 会话信息结构
struct SessionInfo {
    QString session_id;
    QString title;
    QString created_at;
    int message_count;
};

class AIChatWorker;

class AIChatPage : public QWidget
{
    Q_OBJECT

public:
    explicit AIChatPage(const QString &username, QWidget *parent = nullptr);
    ~AIChatPage();  // 析构函数
    void loadChatHistory();  // 加载对话历史
    void clearChat();        // 清空当前对话（新建会话）
    void loadSessionList();  // 加载会话列表

signals:
    void messageSent();      // 消息发送信号
    void startRequest(const QString &jsonData, const QString &host, quint16 port);  // 启动网络请求

protected:
    void resizeEvent(QResizeEvent *event) override;  // 窗口大小变化时重新计算item大小

private slots:
    void onSendClicked();    // 发送按钮点击
    void onInputReturnPressed();  // 输入框回车
    void onSessionItemClicked(QListWidgetItem *item);  // 会话项点击
    void onDeleteSessionClicked();  // 删除会话按钮点击
    void onWorkerFinished(const QByteArray &response);  // Worker完成
    void onWorkerError(const QString &error);  // Worker错误

private:
    QString _username;
    QString _currentSessionId;  // 当前会话ID

    // UI组件
    QListWidget *_sessionListWidget;   // 左侧会话列表
    QListWidget *_chatListWidget;      // 对话消息列表
    QLineEdit *_inputLineEdit;         // 输入框
    QPushButton *_sendButton;          // 发送按钮
    QPushButton *_newChatButton;       // 新建对话按钮
    QPushButton *_deleteSessionButton; // 删除会话按钮
    QLabel *_statusLabel;              // 状态标签
    QLabel *_titleLabel;               // 会话标题标签

    // 数据
    QList<ChatMessage> _messages;
    QList<SessionInfo> _sessions;

    // 多线程
    QThread *_workerThread;
    AIChatWorker *_worker;

    // 方法
    void setupUI();
    void setupWorkerThread();  // 初始化工作线程
    void addMessage(const ChatMessage &msg);
    void addMessageToUI(const ChatMessage &msg);
    void sendMessage(const QString &message);
    void switchSession(const QString &session_id);  // 切换会话
    void deleteCurrentSession();  // 删除当前会话
    void updateSessionTitle(const QString &title);  // 更新会话标题
    QString getCurrentTime() const;
};

#endif // AICHATPAGE_H
