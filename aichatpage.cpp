#include "aichatpage.h"
#include "connectmanager.h"
#include "config.h"

#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QDebug>
#include <QMessageBox>
#include <QScrollBar>

AIChatPage::AIChatPage(const QString &username, QWidget *parent)
    : QWidget(parent)
    , _username(username)
    , _currentSessionId("")
{
    setupUI();
    loadChatHistory();
}

void AIChatPage::setupUI()
{
    setStyleSheet("background-color: white;");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(15);

    // 顶部工具栏
    QHBoxLayout *topBarLayout = new QHBoxLayout();

    _newChatButton = new QPushButton("📝 新建对话", this);
    _newChatButton->setStyleSheet(R"(
        QPushButton {
            background-color: #3498db;
            color: white;
            border: none;
            padding: 8px 16px;
            border-radius: 5px;
            font-size: 14px;
        }
        QPushButton:hover {
            background-color: #2980b9;
        }
        QPushButton:disabled {
            background-color: #bdc3c7;
        }
    )");
    connect(_newChatButton, &QPushButton::clicked, this, &AIChatPage::clearChat);
    topBarLayout->addWidget(_newChatButton);

    _statusLabel = new QLabel("AI 学习助手", this);
    _statusLabel->setStyleSheet("color: #7f8c8d; font-size: 12px;");
    topBarLayout->addStretch();
    topBarLayout->addWidget(_statusLabel);

    mainLayout->addLayout(topBarLayout);

    // 对话显示区域
    _chatListWidget = new QListWidget(this);
    _chatListWidget->setStyleSheet(R"(
        QListWidget {
            border: 1px solid #ddd;
            border-radius: 10px;
            background-color: #f8f9fa;
            padding: 10px;
        }
        QListWidget::item {
            border: none;
            padding: 10px;
            margin: 5px 0px;
            border-radius: 8px;
        }
    )");
    _chatListWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    mainLayout->addWidget(_chatListWidget);

    // 底部输入区域
    QWidget *inputWidget = new QWidget(this);
    inputWidget->setStyleSheet(R"(
        QWidget {
            background-color: #f8f9fa;
            border-radius: 10px;
            border: 1px solid #ddd;
        }
    )");

    QHBoxLayout *inputLayout = new QHBoxLayout(inputWidget);
    inputLayout->setContentsMargins(15, 10, 15, 10);
    inputLayout->setSpacing(10);

    _inputLineEdit = new QLineEdit(this);
    _inputLineEdit->setPlaceholderText("输入你的问题...");
    _inputLineEdit->setStyleSheet(R"(
        QLineEdit {
            border: none;
            background-color: transparent;
            padding: 8px;
            font-size: 14px;
            color: #2c3e50;
        }
        QLineEdit:focus {
            outline: none;
        }
    )");
    _inputLineEdit->setMaxLength(1000);
    connect(_inputLineEdit, &QLineEdit::returnPressed, this, &AIChatPage::onInputReturnPressed);
    inputLayout->addWidget(_inputLineEdit, 1);

    _sendButton = new QPushButton("发送", this);
    _sendButton->setStyleSheet(R"(
        QPushButton {
            background-color: #3498db;
            color: white;
            border: none;
            padding: 8px 20px;
            border-radius: 5px;
            font-size: 14px;
        }
        QPushButton:hover {
            background-color: #2980b9;
        }
        QPushButton:pressed {
            background-color: #21618c;
        }
        QPushButton:disabled {
            background-color: #bdc3c7;
        }
    )");
    connect(_sendButton, &QPushButton::clicked, this, &AIChatPage::onSendClicked);
    inputLayout->addWidget(_sendButton);

    mainLayout->addWidget(inputWidget);

    // 欢迎消息
    if (_messages.isEmpty()) {
        ChatMessage welcomeMsg;
        welcomeMsg.role = "assistant";
        welcomeMsg.content = "你好！我是 SmartLearn 的 AI 学习助手。\n\n我可以帮助你：\n• 根据你的知识库推荐学习内容\n• 回答你的学习疑问\n• 规划个性化学习路径\n\n请输入你的问题开始对话！";
        welcomeMsg.timestamp = getCurrentTime();
        addMessage(welcomeMsg);
    }
}

QString AIChatPage::getCurrentTime() const
{
    return QDateTime::currentDateTime().toString("HH:mm");
}

void AIChatPage::addMessage(const ChatMessage &msg)
{
    _messages.append(msg);
    addMessageToUI(msg);
}

void AIChatPage::addMessageToUI(const ChatMessage &msg)
{
    QListWidgetItem *item = new QListWidgetItem();

    QWidget *widget = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(4);

    // 创建消息头部（角色和时间）
    QHBoxLayout *headerLayout = new QHBoxLayout();
    QLabel *roleLabel = new QLabel(msg.role == "user" ? "👤 你" : "🤖 AI助手", widget);
    roleLabel->setStyleSheet(msg.role == "user"
        ? "color: #3498db; font-size: 12px; font-weight: bold;"
        : "color: #27ae60; font-size: 12px; font-weight: bold;");
    headerLayout->addWidget(roleLabel);

    QLabel *timeLabel = new QLabel(msg.timestamp, widget);
    timeLabel->setStyleSheet("color: #95a5a6; font-size: 11px;");
    headerLayout->addWidget(timeLabel);
    headerLayout->addStretch();

    layout->addLayout(headerLayout);

    // 创建消息内容
    QLabel *contentLabel = new QLabel(msg.content, widget);
    contentLabel->setWordWrap(true);
    contentLabel->setStyleSheet(R"(
        QLabel {
            color: #2c3e50;
            font-size: 14px;
            line-height: 1.4;
        }
    )");
    layout->addWidget(contentLabel);

    // 设置不同角色的样式
    if (msg.role == "user") {
        widget->setStyleSheet(R"(
            QWidget {
                background-color: #e3f2fd;
                border-radius: 8px;
                padding: 5px;
            }
        )");
    } else {
        widget->setStyleSheet(R"(
            QWidget {
                background-color: #e8f5e9;
                border-radius: 8px;
                padding: 5px;
            }
        )");
    }

    item->setSizeHint(widget->sizeHint());
    _chatListWidget->addItem(item);
    _chatListWidget->setItemWidget(item, widget);

    // 滚动到底部
    _chatListWidget->scrollToBottom();
}

void AIChatPage::onInputReturnPressed()
{
    if (!_inputLineEdit->text().trimmed().isEmpty()) {
        onSendClicked();
    }
}

void AIChatPage::onSendClicked()
{
    QString message = _inputLineEdit->text().trimmed();
    if (message.isEmpty()) {
        return;
    }

    _inputLineEdit->clear();
    _sendButton->setEnabled(false);
    _statusLabel->setText("正在思考...");

    // 添加用户消息到UI
    ChatMessage userMsg;
    userMsg.role = "user";
    userMsg.content = message;
    userMsg.timestamp = getCurrentTime();
    addMessage(userMsg);

    // 发送消息到服务器
    sendMessage(message);
}

void AIChatPage::sendMessage(const QString &message)
{
    // 获取socket连接
    ConnectManager &manager = ConnectManager::getInstance();
    QTcpSocket *client = manager.getSocket();

    // 确保socket已连接
    if (client->state() != QAbstractSocket::ConnectedState) {
        qDebug() << "AIChatPage: Socket未连接，尝试重新连接";
        client->abort();
        client->connectToHost(HOSTNAME, PORT);
        if (!client->waitForConnected(3000)) {
            QMessageBox::warning(this, "连接错误", "无法连接到服务器");
            _sendButton->setEnabled(true);
            _statusLabel->setText("连接失败");
            return;
        }
    }

    // 断开所有readyRead信号连接
    disconnect(client, &QTcpSocket::readyRead, nullptr, nullptr);

    // 构造AI聊天请求
    QJsonObject json;
    json["type"] = AIChatType;
    json["username"] = _username;
    json["message"] = message;
    json["session_id"] = _currentSessionId;

    QJsonDocument doc(json);
    QByteArray data = doc.toJson();

    qDebug() << "AIChatPage: 发送消息，会话ID:" << (_currentSessionId.isEmpty() ? "(新建)" : _currentSessionId);

    // 发送请求
    client->write(data);
    client->flush();

    // 等待响应
    if (client->waitForReadyRead(30000)) {  // 30秒超时
        QByteArray responseData = client->readAll();
        qDebug() << "AIChatPage: 收到响应" << responseData.left(200);

        QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);
        if (!responseDoc.isNull() && responseDoc.isObject()) {
            QJsonObject responseJson = responseDoc.object();
            QString type = responseJson["type"].toString();
            QString status = responseJson["status"].toString();

            if (type == "AIChatResponse" && status == "success") {
                QString content = responseJson["content"].toString();
                _currentSessionId = responseJson["session_id"].toString();

                // 添加AI回复
                ChatMessage aiMsg;
                aiMsg.role = "assistant";
                aiMsg.content = content;
                aiMsg.timestamp = getCurrentTime();
                addMessage(aiMsg);

                _statusLabel->setText("在线");
            } else {
                QString errorMsg = responseJson["message"].toString();
                qDebug() << "AI请求失败:" << errorMsg;

                // 添加错误消息
                ChatMessage errorMsgObj;
                errorMsgObj.role = "assistant";
                errorMsgObj.content = "抱歉，AI 服务暂时无法响应：" + errorMsg;
                errorMsgObj.timestamp = getCurrentTime();
                addMessage(errorMsgObj);

                _statusLabel->setText("服务错误");
            }
        } else {
            qDebug() << "解析AI响应失败";
            _statusLabel->setText("响应解析失败");
        }
    } else {
        qDebug() << "AI响应超时";
        ChatMessage timeoutMsg;
        timeoutMsg.role = "assistant";
        timeoutMsg.content = "抱歉，请求超时，请稍后重试。";
        timeoutMsg.timestamp = getCurrentTime();
        addMessage(timeoutMsg);
        _statusLabel->setText("请求超时");
    }

    _sendButton->setEnabled(true);
}

void AIChatPage::loadChatHistory()
{
    // 获取socket连接
    ConnectManager &manager = ConnectManager::getInstance();
    QTcpSocket *client = manager.getSocket();

    // 确保socket已连接
    if (client->state() != QAbstractSocket::ConnectedState) {
        client->abort();
        client->connectToHost(HOSTNAME, PORT);
        if (!client->waitForConnected(3000)) {
            qDebug() << "加载对话历史：连接失败";
            return;
        }
    }

    // 断开所有readyRead信号连接
    disconnect(client, &QTcpSocket::readyRead, nullptr, nullptr);

    // 构造获取对话历史请求
    QJsonObject json;
    json["type"] = GetAIChatHistoryType;
    json["username"] = _username;
    json["session_id"] = "";  // 获取最后一次会话
    json["limit"] = 50;

    QJsonDocument doc(json);
    QByteArray data = doc.toJson();

    qDebug() << "加载对话历史...";

    // 发送请求
    client->write(data);
    client->flush();

    // 等待响应
    if (client->waitForReadyRead(3000)) {
        QByteArray responseData = client->readAll();
        qDebug() << "收到对话历史响应";

        QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);
        if (!responseDoc.isNull() && responseDoc.isObject()) {
            QJsonObject responseJson = responseDoc.object();
            QString status = responseJson["status"].toString();

            if (status == "success") {
                _currentSessionId = responseJson["last_session_id"].toString();
                QJsonArray historyArray = responseJson["history"].toArray();

                _chatListWidget->clear();
                _messages.clear();

                if (historyArray.isEmpty()) {
                    // 显示欢迎消息
                    ChatMessage welcomeMsg;
                    welcomeMsg.role = "assistant";
                    welcomeMsg.content = "你好！我是 SmartLearn 的 AI 学习助手。\n\n我可以帮助你：\n• 根据你的知识库推荐学习内容\n• 回答你的学习疑问\n• 规划个性化学习路径\n\n请输入你的问题开始对话！";
                    welcomeMsg.timestamp = getCurrentTime();
                    addMessage(welcomeMsg);
                } else {
                    for (const QJsonValue &value : historyArray) {
                        QJsonObject msgObj = value.toObject();
                        ChatMessage msg;
                        msg.role = msgObj["role"].toString();
                        msg.content = msgObj["content"].toString();
                        msg.timestamp = getCurrentTime();  // 使用当前时间
                        _messages.append(msg);
                    }

                    // 显示历史消息
                    for (const ChatMessage &msg : _messages) {
                        addMessageToUI(msg);
                    }
                }

                qDebug() << "加载对话历史成功，共" << historyArray.size() << "条记录";
            }
        }
    }
}

void AIChatPage::clearChat()
{
    _currentSessionId = "";
    _chatListWidget->clear();
    _messages.clear();

    // 显示欢迎消息
    ChatMessage welcomeMsg;
    welcomeMsg.role = "assistant";
    welcomeMsg.content = "已开始新对话。\n\n你好！我是 SmartLearn 的 AI 学习助手。\n\n我可以帮助你：\n• 根据你的知识库推荐学习内容\n• 回答你的学习疑问\n• 规划个性化学习路径\n\n请输入你的问题开始对话！";
    welcomeMsg.timestamp = getCurrentTime();
    addMessage(welcomeMsg);

    _inputLineEdit->setFocus();
}
