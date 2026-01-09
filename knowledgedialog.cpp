#include "knowledgedialog.h"
#include "ui_knowledgedialog.h"
#include "connectmanager.h"
#include "config.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QAbstractSocket>

KnowledgeDialog::KnowledgeDialog(const QString &username, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::KnowledgeDialog)
    , _username(username)
{
    ui->setupUi(this);

    setWindowTitle("填写我的知识库");
    setFixedSize(600, 550);
    setModal(false);  // 改为非模态，避免阻塞事件循环
    setWindowModality(Qt::ApplicationModal);  // 但仍然阻止操作其他窗口

    setupUI();

    // 设置样式
    setStyleSheet(R"(
        KnowledgeDialog {
            background-color: #f5f5f5;
        }
        QLabel {
            color: #333;
            font-size: 14px;
        }
        QLabel#title_label {
            font-size: 22px;
            font-weight: bold;
            color: #2196F3;
        }
        QLabel#subtitle_label {
            font-size: 14px;
            color: #666;
        }
        QLineEdit {
            padding: 10px;
            border: 2px solid #ddd;
            border-radius: 6px;
            font-size: 14px;
            background-color: white;
        }
        QLineEdit:focus {
            border: 2px solid #2196F3;
        }
        QListWidget {
            border: 2px solid #ddd;
            border-radius: 6px;
            background-color: white;
            font-size: 14px;
        }
        QListWidget::item {
            padding: 8px;
        }
        QListWidget::item:selected {
            background-color: #E3F2FD;
            color: #2196F3;
        }
        QPushButton {
            padding: 10px 20px;
            border: none;
            border-radius: 6px;
            font-size: 14px;
            font-weight: bold;
        }
        QPushButton#add_btn {
            background-color: #4CAF50;
            color: white;
        }
        QPushButton#add_btn:hover {
            background-color: #45a049;
        }
        QPushButton#remove_btn {
            background-color: #f44336;
            color: white;
        }
        QPushButton#remove_btn:hover {
            background-color: #d32f2f;
        }
        QPushButton#remove_btn:disabled {
            background-color: #cccccc;
            color: #666666;
        }
        QPushButton#skip_btn {
            background-color: #9E9E9E;
            color: white;
        }
        QPushButton#skip_btn:hover {
            background-color: #757575;
        }
        QPushButton#save_btn {
            background-color: #2196F3;
            color: white;
        }
        QPushButton#save_btn:hover {
            background-color: #1976D2;
        }
    )");

    // 获取Socket连接
    ConnectManager &manager = ConnectManager::getInstance();
    _client = manager.getSocket();

    qDebug() << "=== KnowledgeDialog构造 ===";
    qDebug() << "当前socket状态:" << _client->state();

    // 确保socket已连接
    if (_client->state() != QAbstractSocket::ConnectedState) {
        qDebug() << "Socket未连接，尝试重新连接";
        _client->abort();
        _client->connectToHost("127.0.0.1", 8080);
        if (!_client->waitForConnected(3000)) {
            qDebug() << "Socket连接失败:" << _client->errorString();
        } else {
            qDebug() << "Socket重新连接成功";
        }
    }

    // 先断开所有已存在的连接，避免冲突
    disconnect(_client, &QTcpSocket::readyRead, nullptr, nullptr);

    // 连接readyRead信号
    connect(_client, &QTcpSocket::readyRead, this, &KnowledgeDialog::SlotReadFromServer);
    qDebug() << "已连接KnowledgeDialog::SlotReadFromServer到readyRead信号";

    // 加载已有知识点
    loadKnowledge();
}

KnowledgeDialog::~KnowledgeDialog()
{
    // 断开信号连接
    if (_client) {
        disconnect(_client, &QTcpSocket::readyRead, this, &KnowledgeDialog::SlotReadFromServer);
    }
    delete ui;
}

void KnowledgeDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(30, 20, 30, 20);

    // 标题
    QLabel *titleLabel = new QLabel("填写我的知识库", this);
    titleLabel->setObjectName("title_label");
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    // 副标题
    QLabel *subtitleLabel = new QLabel("告诉我们你已经掌握的知识，AI将为你规划学习路径", this);
    subtitleLabel->setObjectName("subtitle_label");
    subtitleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(subtitleLabel);

    // 学习目标
    QLabel *goalLabel = new QLabel("学习目标 (选填)", this);
    mainLayout->addWidget(goalLabel);

    _goal_edit = new QLineEdit(this);
    _goal_edit->setPlaceholderText("例如：考研、找工作、学习AI开发...");
    mainLayout->addWidget(_goal_edit);

    // 知识点输入区域
    QHBoxLayout *inputLayout = new QHBoxLayout();
    _knowledge_edit = new QLineEdit(this);
    _knowledge_edit->setPlaceholderText("输入已掌握的知识点，如：C++、Python、数据结构...");

    _add_btn = new QPushButton("添加", this);
    _add_btn->setObjectName("add_btn");
    _add_btn->setMinimumWidth(80);

    inputLayout->addWidget(_knowledge_edit);
    inputLayout->addWidget(_add_btn);
    mainLayout->addLayout(inputLayout);

    // 知识点列表
    QHBoxLayout *listHeaderLayout = new QHBoxLayout();
    QLabel *listLabel = new QLabel("已添加的知识点：", this);
    _count_label = new QLabel("共 0 个", this);
    _count_label->setStyleSheet("color: #2196F3; font-weight: bold;");
    listHeaderLayout->addWidget(listLabel);
    listHeaderLayout->addStretch();
    listHeaderLayout->addWidget(_count_label);
    mainLayout->addLayout(listHeaderLayout);

    _knowledge_list = new QListWidget(this);
    _knowledge_list->setMaximumHeight(200);
    mainLayout->addWidget(_knowledge_list);

    // 删除按钮
    _remove_btn = new QPushButton("删除选中的知识点", this);
    _remove_btn->setObjectName("remove_btn");
    _remove_btn->setEnabled(false);
    mainLayout->addWidget(_remove_btn);

    // 按钮区域
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(15);
    btnLayout->addStretch();

    _skip_btn = new QPushButton("跳过", this);
    _skip_btn->setObjectName("skip_btn");
    _skip_btn->setMinimumWidth(100);

    _save_btn = new QPushButton("保存并继续", this);
    _save_btn->setObjectName("save_btn");
    _save_btn->setMinimumWidth(120);

    btnLayout->addWidget(_skip_btn);
    btnLayout->addWidget(_save_btn);
    btnLayout->addStretch();

    mainLayout->addLayout(btnLayout);

    // 连接信号槽
    connect(_add_btn, &QPushButton::clicked,
            this, &KnowledgeDialog::onAddKnowledge);
    connect(_remove_btn, &QPushButton::clicked,
            this, &KnowledgeDialog::onRemoveKnowledge);
    connect(_save_btn, &QPushButton::clicked,
            this, &KnowledgeDialog::onSave);
    connect(_skip_btn, &QPushButton::clicked,
            this, &KnowledgeDialog::onSkip);
    connect(_knowledge_list, &QListWidget::itemSelectionChanged,
            this, [this]() {
                _remove_btn->setEnabled(_knowledge_list->currentItem() != nullptr);
            });
    connect(_knowledge_edit, &QLineEdit::returnPressed,
            this, &KnowledgeDialog::onAddKnowledge);
}

void KnowledgeDialog::onAddKnowledge()
{
    QString text = _knowledge_edit->text().trimmed();
    if (text.isEmpty()) {
        return;
    }

    // 检查是否已存在
    for (int i = 0; i < _knowledge_list->count(); ++i) {
        if (_knowledge_list->item(i)->text() == text) {
            QMessageBox::information(this, "提示", "该知识点已存在");
            return;
        }
    }

    _knowledge_list->addItem(text);
    _knowledge_edit->clear();
    _knowledge_edit->setFocus();

    // 更新计数
    _count_label->setText(QString("共 %1 个").arg(_knowledge_list->count()));
}

void KnowledgeDialog::onRemoveKnowledge()
{
    QListWidgetItem *item = _knowledge_list->currentItem();
    if (item) {
        delete item;
        _count_label->setText(QString("共 %1 个").arg(_knowledge_list->count()));
    }
}

void KnowledgeDialog::onSave()
{
    // 收集知识点
    QJsonArray knowledgeArray;
    for (int i = 0; i < _knowledge_list->count(); ++i) {
        knowledgeArray.append(_knowledge_list->item(i)->text());
    }

    // 构造JSON请求
    QJsonObject json;
    json["type"] = SaveKnowledgeType;
    json["username"] = _username;
    json["learning_goal"] = _goal_edit->text().trimmed();
    json["knowledge_points"] = knowledgeArray;

    QJsonDocument doc(json);

    // 检查socket连接状态
    qDebug() << "当前socket状态:" << _client->state();
    if (_client->state() != QAbstractSocket::ConnectedState) {
        qDebug() << "Socket未连接，尝试重新连接";
        _client->abort();
        _client->connectToHost("127.0.0.1", 8080);
        if (!_client->waitForConnected(3000)) {
            QMessageBox::warning(this, "连接错误", "无法连接到服务器");
            return;
        }
        qDebug() << "重新连接成功，状态:" << _client->state();
    }

    // 禁用按钮
    _save_btn->setEnabled(false);
    _skip_btn->setEnabled(false);
    _save_btn->setText("保存中...");

    // 发送前断开信号，避免 SlotReadFromServer() 与 waitForReadyRead() 冲突
    disconnect(_client, &QTcpSocket::readyRead, this, &KnowledgeDialog::SlotReadFromServer);

    // 发送请求
    QByteArray data = doc.toJson();
    qDebug() << "发送保存知识库请求:" << _username;
    qDebug() << "请求数据:" << data;

    qint64 bytesWritten = _client->write(data);
    _client->flush();

    qDebug() << "已写入" << bytesWritten << "字节";

    // 直接等待响应，不依赖信号
    if (_client->waitForReadyRead(5000)) {
        QByteArray responseData = _client->readAll();
        qDebug() << "收到响应数据:" << responseData;

        QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);
        if (!responseDoc.isNull() && responseDoc.isObject()) {
            QJsonObject responseJson = responseDoc.object();
            QString type = responseJson["type"].toString();
            QString status = responseJson["status"].toString();
            QString message = responseJson["message"].toString();
            qDebug() << "响应类型:" << type << "状态:" << status;

            if (type == "KnowledgeResponse" && status == "success") {
                QMessageBox::information(this, "保存成功", message);
                accept();
            } else {
                QMessageBox::warning(this, "保存失败", message);
                _save_btn->setEnabled(true);
                _skip_btn->setEnabled(true);
                _save_btn->setText("保存并继续");
                // 重新连接信号，以便下次操作
                connect(_client, &QTcpSocket::readyRead, this, &KnowledgeDialog::SlotReadFromServer);
            }
        } else {
            qDebug() << "响应解析失败";
            QMessageBox::warning(this, "错误", "服务器响应格式错误");
            _save_btn->setEnabled(true);
            _skip_btn->setEnabled(true);
            _save_btn->setText("保存并继续");
            // 重新连接信号，以便下次操作
            connect(_client, &QTcpSocket::readyRead, this, &KnowledgeDialog::SlotReadFromServer);
        }
    } else {
        qDebug() << "等待响应超时";
        QMessageBox::warning(this, "超时", "服务器无响应，请检查网络连接");
        _save_btn->setEnabled(true);
        _skip_btn->setEnabled(true);
        _save_btn->setText("保存并继续");
        // 重新连接信号，以便下次操作
        connect(_client, &QTcpSocket::readyRead, this, &KnowledgeDialog::SlotReadFromServer);
    }
}

void KnowledgeDialog::onSkip()
{
    // 直接跳过，不保存任何数据
    accept();
}

void KnowledgeDialog::SlotReadFromServer()
{
    QByteArray data = _client->readAll();
    qDebug() << "=== KnowledgeDialog::SlotReadFromServer 被调用 ===";
    qDebug() << "响应数据:" << data;
    qDebug() << "响应长度:" << data.length();

    if (data.isEmpty()) {
        qDebug() << "收到空数据，忽略";
        return;
    }

    // 先尝试解析JSON
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        qDebug() << "不是JSON格式，可能是登录响应或其他非知识库响应，忽略";
        return;  // 不是知识库相关的JSON响应，忽略
    }

    QJsonObject json = doc.object();
    QString type = json["type"].toString();
    qDebug() << "响应类型:" << type;

    if (type != "KnowledgeResponse") {
        qDebug() << "不是KnowledgeResponse，忽略";
        return;
    }

    QString status = json["status"].toString();
    QString message = json["message"].toString();
    qDebug() << "状态:" << status << "消息:" << message;

    // 恢复按钮状态
    _save_btn->setEnabled(true);
    _skip_btn->setEnabled(true);
    _save_btn->setText("保存并继续");

    if (status == "success") {
        QMessageBox::information(this, "保存成功", message);
        accept();  // 关闭对话框，进入主窗口
    } else {
        QMessageBox::warning(this, "保存失败", message);
    }
}

void KnowledgeDialog::loadKnowledge()
{
    qDebug() << "=== loadKnowledge 开始 ===";

    // 构造获取知识库请求
    QJsonObject json;
    json["type"] = GetKnowledgeType;
    json["username"] = _username;

    QJsonDocument doc(json);
    QByteArray data = doc.toJson();

    qDebug() << "发送获取知识库请求:" << _username;

    // 断开信号，使用同步等待
    disconnect(_client, &QTcpSocket::readyRead, this, &KnowledgeDialog::SlotReadFromServer);

    // 发送请求
    _client->write(data);
    _client->flush();

    // 等待响应
    if (_client->waitForReadyRead(3000)) {
        QByteArray responseData = _client->readAll();
        qDebug() << "收到知识库数据:" << responseData;

        QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);
        if (!responseDoc.isNull() && responseDoc.isObject()) {
            QJsonObject responseJson = responseDoc.object();
            QString type = responseJson["type"].toString();
            QString status = responseJson["status"].toString();

            if (type == "KnowledgeResponse" && status == "success") {
                QJsonArray knowledgeArray = responseJson["knowledge_points"].toArray();

                // 清空列表
                _knowledge_list->clear();

                // 填充知识点
                for (const QJsonValue &value : knowledgeArray) {
                    QString point = value.toString();
                    _knowledge_list->addItem(point);
                }

                // 更新计数
                _count_label->setText(QString("共 %1 个").arg(_knowledge_list->count()));

                qDebug() << "知识库加载成功，共" << knowledgeArray.size() << "个知识点";

                // 尝试获取学习目标
                if (responseJson.contains("learning_goal")) {
                    _goal_edit->setText(responseJson["learning_goal"].toString());
                }
            } else {
                qDebug() << "获取知识库失败:" << responseJson["message"].toString();
            }
        } else {
            qDebug() << "响应解析失败";
        }
    } else {
        qDebug() << "获取知识库超时";
    }

    // 重连信号
    connect(_client, &QTcpSocket::readyRead, this, &KnowledgeDialog::SlotReadFromServer);

    qDebug() << "=== loadKnowledge 结束 ===";
}
