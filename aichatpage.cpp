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
    loadSessionList();   // 先加载会话列表
    loadChatHistory();   // 再加载聊天历史（加载最后一次会话）
}

void AIChatPage::setupUI()
{
    setStyleSheet("background-color: white;");

    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ========== 左侧会话列表区域 ==========
    QWidget *leftPanel = new QWidget(this);
    leftPanel->setFixedWidth(250);
    leftPanel->setStyleSheet(R"(
        QWidget {
            background-color: #f5f5f5;
            border-right: 1px solid #e0e0e0;
        }
    )");

    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(10, 10, 10, 10);
    leftLayout->setSpacing(10);

    // 新建对话按钮
    _newChatButton = new QPushButton("📝 新建对话", leftPanel);
    _newChatButton->setStyleSheet(R"(
        QPushButton {
            background-color: #3498db;
            color: white;
            border: none;
            padding: 10px;
            border-radius: 6px;
            font-size: 14px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: #2980b9;
        }
        QPushButton:pressed {
            background-color: #21618c;
        }
    )");
    connect(_newChatButton, &QPushButton::clicked, this, &AIChatPage::clearChat);
    leftLayout->addWidget(_newChatButton);

    // 会话列表标题
    QLabel *sessionListTitle = new QLabel("历史会话", leftPanel);
    sessionListTitle->setStyleSheet("color: #7f8c8d; font-size: 12px; font-weight: bold; padding: 5px;");
    leftLayout->addWidget(sessionListTitle);

    // 会话列表
    _sessionListWidget = new QListWidget(leftPanel);
    _sessionListWidget->setStyleSheet(R"(
        QListWidget {
            border: none;
            background-color: transparent;
            padding: 5px;
        }
        QListWidget::item {
            border: none;
            padding: 12px 10px;
            margin: 2px 0px;
            border-radius: 6px;
            background-color: white;
            color: #2c3e50;
            font-size: 13px;
        }
        QListWidget::item:hover {
            background-color: #e8f4fd;
        }
        QListWidget::item:selected {
            background-color: #3498db;
            color: white;
        }
    )");
    _sessionListWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    connect(_sessionListWidget, &QListWidget::itemClicked, this, &AIChatPage::onSessionItemClicked);
    leftLayout->addWidget(_sessionListWidget);

    mainLayout->addWidget(leftPanel);

    // ========== 右侧对话区域 ==========
    QWidget *rightPanel = new QWidget(this);
    QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(20, 15, 20, 15);
    rightLayout->setSpacing(10);

    // 顶部工具栏
    QHBoxLayout *topBarLayout = new QHBoxLayout();

    _titleLabel = new QLabel("AI 学习助手", rightPanel);
    _titleLabel->setStyleSheet("color: #2c3e50; font-size: 16px; font-weight: bold;");
    topBarLayout->addWidget(_titleLabel);

    topBarLayout->addStretch();

    _deleteSessionButton = new QPushButton("🗑️ 删除会话", rightPanel);
    _deleteSessionButton->setStyleSheet(R"(
        QPushButton {
            background-color: #e74c3c;
            color: white;
            border: none;
            padding: 6px 12px;
            border-radius: 4px;
            font-size: 12px;
        }
        QPushButton:hover {
            background-color: #c0392b;
        }
        QPushButton:disabled {
            background-color: #bdc3c7;
        }
    )");
    _deleteSessionButton->setEnabled(false);  // 默认禁用，有会话时启用
    connect(_deleteSessionButton, &QPushButton::clicked, this, &AIChatPage::onDeleteSessionClicked);
    topBarLayout->addWidget(_deleteSessionButton);

    _statusLabel = new QLabel("● 在线", rightPanel);
    _statusLabel->setStyleSheet("color: #27ae60; font-size: 12px;");
    topBarLayout->addWidget(_statusLabel);

    rightLayout->addLayout(topBarLayout);

    // 对话显示区域
    _chatListWidget = new QListWidget(rightPanel);
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
    rightLayout->addWidget(_chatListWidget);

    // 底部输入区域
    QWidget *inputWidget = new QWidget(rightPanel);
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

    _inputLineEdit = new QLineEdit(rightPanel);
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

    _sendButton = new QPushButton("发送", rightPanel);
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

    rightLayout->addWidget(inputWidget);

    mainLayout->addWidget(rightPanel, 1);
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

    // 设置边距 - 增加上下边距，确保单行消息不被遮挡
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
    contentLabel->setObjectName("contentLabel");
    // contentLabel->setMaximumWidth(_chatListWidget->width() - 50);  // 注释：导致首次加载时文字显示异常（width()返回默认值约100）
    contentLabel->setMinimumWidth(700);
    contentLabel->setWordWrap(true);
    contentLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    contentLabel->setStyleSheet(R"(
        QLabel {
            color: #2c3e50;
            font-size: 14px;
            background-color: transparent;
            padding: 4px 0px;
        }
    )");

    layout->addWidget(contentLabel);

    // 设置不同角色的样式
    if (msg.role == "user") {
        widget->setStyleSheet(R"(
            QWidget {
                background-color: #e3f2fd;
                border-radius: 8px;
            }
        )");
    } else {
        widget->setStyleSheet(R"(
            QWidget {
                background-color: #e8f5e9;
                border-radius: 8px;
            }
        )");
    }

    // 关键修复：直接设置widget的最小高度，确保内容不被裁剪
    widget->setMinimumHeight(70);

    // 激活布局并调整大小
    layout->activate();
    widget->adjustSize();

    // 设置item大小 - 使用widget的实际大小而不是sizeHint
    QSize hint = widget->size();

    item->setSizeHint(QSize(_chatListWidget->width() - 25, hint.height()));

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

    _titleLabel->setText("新对话");
    _deleteSessionButton->setEnabled(false);

    // 显示欢迎消息
    ChatMessage welcomeMsg;
    welcomeMsg.role = "assistant";
    welcomeMsg.content = "已开始新对话。\n\n你好！我是 SmartLearn 的 AI 学习助手。\n\n我可以帮助你：\n• 根据你的知识库推荐学习内容\n• 回答你的学习疑问\n• 规划个性化学习路径\n\n请输入你的问题开始对话！";
    welcomeMsg.timestamp = getCurrentTime();
    addMessage(welcomeMsg);

    _inputLineEdit->setFocus();

    // 重新加载会话列表
    loadSessionList();
}

// ========== 会话管理相关 ==========

void AIChatPage::loadSessionList()
{
    ConnectManager &manager = ConnectManager::getInstance();
    QTcpSocket *client = manager.getSocket();

    if (client->state() != QAbstractSocket::ConnectedState) {
        client->abort();
        client->connectToHost(HOSTNAME, PORT);
        if (!client->waitForConnected(3000)) {
            qDebug() << "加载会话列表：连接失败";
            return;
        }
    }

    disconnect(client, &QTcpSocket::readyRead, nullptr, nullptr);

    QJsonObject json;
    json["type"] = GetSessionListType;
    json["username"] = _username;

    QJsonDocument doc(json);
    QByteArray data = doc.toJson();

    qDebug() << "请求会话列表...";

    client->write(data);
    client->flush();

    if (client->waitForReadyRead(3000)) {
        QByteArray responseData = client->readAll();

        QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);
        if (!responseDoc.isNull() && responseDoc.isObject()) {
            QJsonObject responseJson = responseDoc.object();

            if (responseJson["status"].toString() == "success") {
                _sessions.clear();
                _sessionListWidget->clear();

                QJsonArray sessionArray = responseJson["sessions"].toArray();
                for (const QJsonValue &value : sessionArray) {
                    QJsonObject sessionObj = value.toObject();
                    SessionInfo info;
                    info.session_id = sessionObj["session_id"].toString();
                    info.title = sessionObj["title"].toString();
                    info.created_at = sessionObj["created_at"].toString();
                    info.message_count = sessionObj["message_count"].toInt();
                    _sessions.append(info);

                    // 添加到列表
                    QListWidgetItem *item = new QListWidgetItem();
                    QString displayText = info.title;
                    if (!displayText.isEmpty() && displayText != "新对话") {
                        displayText += "\n(" + QString::number(info.message_count) + " 条消息)";
                    }
                    item->setText(displayText);
                    item->setData(Qt::UserRole, info.session_id);
                    _sessionListWidget->addItem(item);

                    // 高亮当前会话
                    if (info.session_id == _currentSessionId) {
                        item->setSelected(true);
                    }
                }

                qDebug() << "会话列表加载成功，共" << _sessions.size() << "个会话";
            }
        }
    }
}

void AIChatPage::onSessionItemClicked(QListWidgetItem *item)
{
    if (!item) return;

    QString sessionId = item->data(Qt::UserRole).toString();
    if (sessionId.isEmpty()) return;

    switchSession(sessionId);
}

void AIChatPage::switchSession(const QString &session_id)
{
    if (session_id == _currentSessionId) return;

    _currentSessionId = session_id;

    // 查找会话标题
    for (const SessionInfo &info : _sessions) {
        if (info.session_id == session_id) {
            _titleLabel->setText(info.title);
            break;
        }
    }
    _deleteSessionButton->setEnabled(true);

    // 加载该会话的历史消息
    ConnectManager &manager = ConnectManager::getInstance();
    QTcpSocket *client = manager.getSocket();

    if (client->state() != QAbstractSocket::ConnectedState) {
        client->abort();
        client->connectToHost(HOSTNAME, PORT);
        if (!client->waitForConnected(3000)) {
            qDebug() << "切换会话：连接失败";
            return;
        }
    }

    disconnect(client, &QTcpSocket::readyRead, nullptr, nullptr);

    QJsonObject json;
    json["type"] = GetAIChatHistoryType;
    json["username"] = _username;
    json["session_id"] = session_id;
    json["limit"] = 50;

    QJsonDocument doc(json);
    client->write(doc.toJson());
    client->flush();

    if (client->waitForReadyRead(3000)) {
        QByteArray responseData = client->readAll();

        QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);
        if (!responseDoc.isNull() && responseDoc.isObject()) {
            QJsonObject responseJson = responseDoc.object();

            if (responseJson["status"].toString() == "success") {
                _chatListWidget->clear();
                _messages.clear();

                QJsonArray historyArray = responseJson["history"].toArray();
                for (const QJsonValue &value : historyArray) {
                    QJsonObject msgObj = value.toObject();
                    ChatMessage msg;
                    msg.role = msgObj["role"].toString();
                    msg.content = msgObj["content"].toString();
                    msg.timestamp = getCurrentTime();
                    _messages.append(msg);
                    addMessageToUI(msg);
                }

                qDebug() << "会话切换成功，加载" << historyArray.size() << "条消息";
            }
        }
    }

    // 滚动到底部
    _chatListWidget->scrollToBottom();
}

void AIChatPage::onDeleteSessionClicked()
{
    if (_currentSessionId.isEmpty()) {
        QMessageBox::warning(this, "提示", "当前没有选中的会话");
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "确认删除",
        "确定要删除当前会话吗？此操作无法撤销。",
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        deleteCurrentSession();
    }
}

void AIChatPage::deleteCurrentSession()
{
    ConnectManager &manager = ConnectManager::getInstance();
    QTcpSocket *client = manager.getSocket();

    if (client->state() != QAbstractSocket::ConnectedState) {
        client->abort();
        client->connectToHost(HOSTNAME, PORT);
        if (!client->waitForConnected(3000)) {
            qDebug() << "删除会话：连接失败";
            return;
        }
    }

    disconnect(client, &QTcpSocket::readyRead, nullptr, nullptr);

    QJsonObject json;
    json["type"] = DeleteSessionType;
    json["username"] = _username;
    json["session_id"] = _currentSessionId;

    QJsonDocument doc(json);
    client->write(doc.toJson());
    client->flush();

    if (client->waitForReadyRead(3000)) {
        QByteArray responseData = client->readAll();

        QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);
        if (!responseDoc.isNull() && responseDoc.isObject()) {
            QJsonObject responseJson = responseDoc.object();

            if (responseJson["status"].toString() == "success") {
                qDebug() << "会话删除成功";

                // 清空当前会话
                _currentSessionId = "";
                _chatListWidget->clear();
                _messages.clear();
                _titleLabel->setText("AI 学习助手");
                _deleteSessionButton->setEnabled(false);

                // 显示欢迎消息
                ChatMessage welcomeMsg;
                welcomeMsg.role = "assistant";
                welcomeMsg.content = "会话已删除。\n\n你好！我是 SmartLearn 的 AI 学习助手。\n\n我可以帮助你：\n• 根据你的知识库推荐学习内容\n• 回答你的学习疑问\n• 规划个性化学习路径\n\n请输入你的问题开始对话！";
                welcomeMsg.timestamp = getCurrentTime();
                addMessage(welcomeMsg);

                // 重新加载会话列表
                loadSessionList();
            } else {
                QMessageBox::warning(this, "删除失败", responseJson["message"].toString());
            }
        }
    }
}

void AIChatPage::updateSessionTitle(const QString &title)
{
    _titleLabel->setText(title);
}

void AIChatPage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    if (_chatListWidget) {
        int availableWidth = _chatListWidget->width() - 50;

        for (int i = 0; i < _chatListWidget->count(); ++i) {
            QListWidgetItem *item = _chatListWidget->item(i);
            QWidget *widget = _chatListWidget->itemWidget(item);
            if (widget) {
                // 找到 contentLabel 并更新其最大宽度，触发文字重新换行
                QLabel *contentLabel = widget->findChild<QLabel*>("contentLabel");
                if (contentLabel) {
                    contentLabel->setMaximumWidth(availableWidth);
                }

                // 触发布局重新计算
                widget->updateGeometry();

                // 重新计算并设置item大小
                QSize hint = widget->sizeHint();
                if (hint.height() < 70) hint.setHeight(70);
                item->setSizeHint(QSize(_chatListWidget->width() - 25, hint.height()));
            }
        }
    }
}
