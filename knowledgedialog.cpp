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

    // 连接服务器响应信号槽
    disconnect(_client, &QTcpSocket::readyRead, nullptr, nullptr);
    connect(_client, &QTcpSocket::readyRead,
            this, &KnowledgeDialog::SlotReadFromServer);
}

KnowledgeDialog::~KnowledgeDialog()
{
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

    // 检查socket连接
    if (_client->state() != QAbstractSocket::ConnectedState) {
        qDebug() << "Socket未连接，尝试重新连接";
        _client->abort();
        _client->connectToHost("127.0.0.1", 8080);
        if (!_client->waitForConnected(3000)) {
            QMessageBox::warning(this, "连接错误", "无法连接到服务器");
            return;
        }
    }

    // 发送请求
    _client->write(doc.toJson());
    _client->flush();

    qDebug() << "发送保存知识库请求:" << _username;

    // 禁用按钮
    _save_btn->setEnabled(false);
    _skip_btn->setEnabled(false);
    _save_btn->setText("保存中...");
}

void KnowledgeDialog::onSkip()
{
    // 直接跳过，不保存任何数据
    accept();
}

void KnowledgeDialog::SlotReadFromServer()
{
    QByteArray data = _client->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject json = doc.object();

    QString type = json["type"].toString();
    if (type != "KnowledgeResponse") {
        return;
    }

    QString status = json["status"].toString();
    QString message = json["message"].toString();

    // 恢复按钮状态
    _save_btn->setEnabled(true);
    _skip_btn->setEnabled(true);
    _save_btn->setText("保存并继续");

    if (status == "success") {
        QMessageBox::information(this, "保存成功", message);
        accept();  // 关闭对话框
    } else {
        QMessageBox::warning(this, "保存失败", message);
    }
}
