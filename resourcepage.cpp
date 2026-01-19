#include "resourcepage.h"
#include "resourceitem.h"
#include "connectmanager.h"
#include "config.h"

#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMessageBox>
#include <QDebug>
#include <QDesktopServices>
#include <QUrl>

ResourcePage::ResourcePage(const QString &username, QWidget *parent)
    : QWidget(parent)
    , _username(username)
    , _currentPathId(0)
{
    setupUI();
    loadPathList();
}

void ResourcePage::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ========== 顶部工具栏 ==========
    QWidget *topBar = new QWidget(this);
    topBar->setStyleSheet("background-color: #f8f9fa; border-bottom: 1px solid #e0e0e0;");
    QHBoxLayout *topBarLayout = new QHBoxLayout(topBar);
    topBarLayout->setContentsMargins(20, 15, 20, 15);

    _titleLabel = new QLabel("📚 学习资源推荐", topBar);
    _titleLabel->setStyleSheet("color: #2c3e50; font-size: 18px; font-weight: bold;");
    topBarLayout->addWidget(_titleLabel);

    topBarLayout->addStretch();

    // 刷新按钮
    _refreshBtn = new QPushButton("🔄 刷新", topBar);
    _refreshBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #95a5a6;
            color: white;
            border: none;
            padding: 8px 16px;
            border-radius: 6px;
            font-size: 13px;
        }
        QPushButton:hover {
            background-color: #7f8c8d;
        }
        QPushButton:pressed {
            background-color: #6c7a7d;
        }
    )");
    connect(_refreshBtn, &QPushButton::clicked, this, &ResourcePage::onRefreshClicked);
    topBarLayout->addWidget(_refreshBtn);

    mainLayout->addWidget(topBar);

    // ========== 内容区域 ==========
    QWidget *contentWidget = new QWidget(this);
    QVBoxLayout *contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(20, 20, 20, 20);
    contentLayout->setSpacing(15);

    // 路径选择区域
    QWidget *pathSelectWidget = new QWidget(contentWidget);
    QHBoxLayout *pathSelectLayout = new QHBoxLayout(pathSelectWidget);
    pathSelectLayout->setContentsMargins(0, 0, 0, 0);

    QLabel *pathLabel = new QLabel("选择学习路径:", pathSelectWidget);
    pathLabel->setStyleSheet("color: #2c3e50; font-size: 14px; font-weight: bold;");
    pathSelectLayout->addWidget(pathLabel);

    _pathCombo = new QComboBox(pathSelectWidget);
    _pathCombo->setStyleSheet(R"(
        QComboBox {
            border: 1px solid #bdc3c7;
            border-radius: 6px;
            padding: 8px 12px;
            background-color: white;
            font-size: 13px;
            min-width: 250px;
        }
        QComboBox:hover {
            border: 1px solid #3498db;
        }
        QComboBox::drop-down {
            border: none;
        }
        QComboBox::down-arrow {
            image: none;
            border-left: 5px solid transparent;
            border-right: 5px solid transparent;
            border-top: 5px solid #7f8c8d;
            margin-right: 8px;
        }
    )");
    connect(_pathCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ResourcePage::onPathChanged);
    pathSelectLayout->addWidget(_pathCombo);

    pathSelectLayout->addStretch();

    // 生成资源推荐按钮
    _generateBtn = new QPushButton("✨ 生成资源推荐", pathSelectWidget);
    _generateBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #27ae60;
            color: white;
            border: none;
            padding: 10px 20px;
            border-radius: 6px;
            font-size: 13px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: #229954;
        }
        QPushButton:pressed {
            background-color: #1e8449;
        }
        QPushButton:disabled {
            background-color: #bdc3c7;
            color: #7f8c8d;
        }
    )");
    connect(_generateBtn, &QPushButton::clicked, this, &ResourcePage::onGenerateClicked);
    pathSelectLayout->addWidget(_generateBtn);

    contentLayout->addWidget(pathSelectWidget);

    // 状态标签
    _statusLabel = new QLabel(contentWidget);
    _statusLabel->setStyleSheet("color: #7f8c8d; font-size: 12px; padding: 5px;");
    _statusLabel->hide();
    contentLayout->addWidget(_statusLabel);

    // 资源列表
    _resourceList = new QListWidget(contentWidget);
    _resourceList->setStyleSheet(R"(
        QListWidget {
            border: none;
            background-color: transparent;
            padding: 5px;
        }
        QListWidget::item {
            border: 1px solid #e0e0e0;
            border-radius: 8px;
            padding: 10px;
            margin: 5px 0px;
            background-color: white;
        }
        QListWidget::item:hover {
            background-color: #f8f9fa;
            border: 1px solid #3498db;
        }
    )");
    contentLayout->addWidget(_resourceList, 1);

    mainLayout->addWidget(contentWidget);

    // 初始化按钮状态
    updateGenerateButtonState();
}

void ResourcePage::loadPathList()
{
    requestGetPathList();
}

void ResourcePage::refreshResources()
{
    if (_currentPathId > 0) {
        requestGetResources(_currentPathId);
    }
}

void ResourcePage::onPathChanged(int index)
{
    if (index < 0 || index >= _pathList.size()) {
        _currentPathId = 0;
        _currentPathName.clear();
        _resourceList->clear();
        updateGenerateButtonState();
        return;
    }

    _currentPathId = _pathList[index].path_id;
    _currentPathName = _pathList[index].path_name;

    // 自动加载该路径的资源
    requestGetResources(_currentPathId);
    updateGenerateButtonState();
}

void ResourcePage::onGenerateClicked()
{
    if (_currentPathId <= 0) {
        QMessageBox::warning(this, "提示", "请先选择一个学习路径");
        return;
    }

    // 确认对话框
    int ret = QMessageBox::question(this, "确认生成",
        "将为路径 \"" + _currentPathName + "\" 生成AI资源推荐。\n\n"
        "这可能需要一些时间，是否继续？",
        QMessageBox::Yes | QMessageBox::No);

    if (ret != QMessageBox::Yes) {
        return;
    }

    showLoadingState(true);

    // 先获取路径详情以获取阶段信息，然后生成资源
    ConnectManager &manager = ConnectManager::getInstance();
    QTcpSocket *client = manager.getSocket();

    if (client->state() != QAbstractSocket::ConnectedState) {
        client->abort();
        client->connectToHost(HOSTNAME, PORT);
        if (!client->waitForConnected(3000)) {
            QMessageBox::warning(this, "错误", "无法连接到服务器");
            showLoadingState(false);
            return;
        }
    }

    disconnect(client, &QTcpSocket::readyRead, nullptr, nullptr);

    QJsonObject json;
    json["type"] = GetLearningPathDetailType;
    json["username"] = _username;
    json["path_id"] = _currentPathId;

    QJsonDocument doc(json);
    client->write(doc.toJson());
    client->flush();

    if (client->waitForReadyRead(10000)) {
        QByteArray responseData = client->readAll();
        QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);

        if (!responseDoc.isNull() && responseDoc.isObject()) {
            QJsonObject responseJson = responseDoc.object();

            if (responseJson["status"].toString() == "success" && responseJson.contains("path_data")) {
                QJsonObject pathData = responseJson["path_data"].toObject();
                QJsonArray stages = pathData["stages"].toArray();

                // 请求生成资源
                requestGenerateResources(_currentPathId);
            } else {
                QMessageBox::warning(this, "错误", "无法获取路径详情，请先创建学习路径");
                showLoadingState(false);
            }
        }
    } else {
        QMessageBox::warning(this, "错误", "请求超时");
        showLoadingState(false);
    }
}

void ResourcePage::onRefreshClicked()
{
    loadPathList();
}

void ResourcePage::onResourceItemClicked(const QString &url)
{
    openUrl(url);
}

void ResourcePage::SlotReadFromServer()
{
    // 这个页面使用同步通信，不需要异步槽函数
    // 保留此函数以备将来扩展
}

void ResourcePage::requestGenerateResources(int path_id)
{
    ConnectManager &manager = ConnectManager::getInstance();
    QTcpSocket *client = manager.getSocket();

    if (client->state() != QAbstractSocket::ConnectedState) {
        client->abort();
        client->connectToHost(HOSTNAME, PORT);
        if (!client->waitForConnected(3000)) {
            QMessageBox::warning(this, "错误", "无法连接到服务器");
            showLoadingState(false);
            return;
        }
    }

    disconnect(client, &QTcpSocket::readyRead, nullptr, nullptr);

    QJsonObject json;
    json["type"] = GenerateResourcesType;
    json["username"] = _username;
    json["path_id"] = path_id;
    json["path_name"] = _currentPathName;

    // 需要发送阶段数据，这里使用空数组
    // 实际上服务端会从数据库获取
    QJsonArray stages;
    json["stages"] = stages;

    QJsonDocument doc(json);
    client->write(doc.toJson());
    client->flush();

    // 等待响应（AI生成可能需要较长时间）
    if (client->waitForReadyRead(120000)) {  // 2分钟超时
        QByteArray responseData = client->readAll();
        QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);

        if (!responseDoc.isNull() && responseDoc.isObject()) {
            QJsonObject responseJson = responseDoc.object();

            if (responseJson["status"].toString() == "success") {
                QMessageBox::information(this, "成功",
                    responseJson["message"].toString());

                // 重新加载资源列表
                requestGetResources(path_id);
            } else {
                QMessageBox::warning(this, "生成失败",
                    responseJson["message"].toString());
            }
        } else {
            QMessageBox::warning(this, "错误", "服务器响应格式错误");
        }
    } else {
        QMessageBox::warning(this, "错误", "请求超时，AI生成时间过长");
    }

    showLoadingState(false);
}

void ResourcePage::requestGetResources(int path_id)
{
    ConnectManager &manager = ConnectManager::getInstance();
    QTcpSocket *client = manager.getSocket();

    if (client->state() != QAbstractSocket::ConnectedState) {
        client->abort();
        client->connectToHost(HOSTNAME, PORT);
        if (!client->waitForConnected(3000)) {
            qDebug() << "ResourcePage: 连接失败";
            return;
        }
    }

    disconnect(client, &QTcpSocket::readyRead, nullptr, nullptr);

    QJsonObject json;
    json["type"] = GetResourcesType;
    json["username"] = _username;
    json["path_id"] = path_id;

    QJsonDocument doc(json);
    client->write(doc.toJson());
    client->flush();

    if (client->waitForReadyRead(5000)) {
        QByteArray responseData = client->readAll();
        QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);

        if (!responseDoc.isNull() && responseDoc.isObject()) {
            QJsonObject responseJson = responseDoc.object();

            if (responseJson["status"].toString() == "success" && responseJson.contains("resources")) {
                _resources.clear();
                QJsonArray resourcesArray = responseJson["resources"].toArray();

                for (const QJsonValue &val : resourcesArray) {
                    QJsonObject obj = val.toObject();
                    ResourceInfo info;
                    info.id = obj["id"].toInt();
                    info.path_id = obj["path_id"].toInt();
                    info.stage_order = obj["stage_order"].toInt();
                    info.stage_name = "";  // 服务端不返回，需要在UI中填充
                    info.title = obj["title"].toString();
                    info.url = obj["url"].toString();
                    info.source = obj["source"].toString();
                    info.description = obj["description"].toString();
                    info.difficulty = obj["difficulty"].toInt();
                    _resources.append(info);
                }

                displayResources(_resources);
            }
        }
    }
}

void ResourcePage::requestGetPathList()
{
    ConnectManager &manager = ConnectManager::getInstance();
    QTcpSocket *client = manager.getSocket();

    if (client->state() != QAbstractSocket::ConnectedState) {
        client->abort();
        client->connectToHost(HOSTNAME, PORT);
        if (!client->waitForConnected(3000)) {
            qDebug() << "ResourcePage: 连接失败";
            return;
        }
    }

    disconnect(client, &QTcpSocket::readyRead, nullptr, nullptr);

    QJsonObject json;
    json["type"] = GetLearningPathListType;
    json["username"] = _username;

    QJsonDocument doc(json);
    client->write(doc.toJson());
    client->flush();

    if (client->waitForReadyRead(5000)) {
        QByteArray responseData = client->readAll();
        QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);

        if (!responseDoc.isNull() && responseDoc.isObject()) {
            QJsonObject responseJson = responseDoc.object();

            if (responseJson["status"].toString() == "success" && responseJson.contains("paths")) {
                _pathList.clear();
                _pathCombo->clear();

                QJsonArray pathsArray = responseJson["paths"].toArray();

                for (const QJsonValue &val : pathsArray) {
                    QJsonObject obj = val.toObject();
                    PathSimpleInfo info;
                    info.path_id = obj["path_id"].toInt();
                    info.path_name = obj["path_name"].toString();
                    info.learning_goal = obj["learning_goal"].toString();
                    _pathList.append(info);

                    QString displayText = info.path_name;
                    if (!info.learning_goal.isEmpty()) {
                        displayText += " - " + info.learning_goal;
                    }
                    _pathCombo->addItem(displayText);
                }

                qDebug() << "ResourcePage: 加载了" << _pathList.size() << "条学习路径";

                // 如果有路径，默认选择第一个
                if (!_pathList.isEmpty()) {
                    _currentPathId = _pathList[0].path_id;
                    _currentPathName = _pathList[0].path_name;
                    updateGenerateButtonState();
                    // 自动加载第一个路径的资源
                    requestGetResources(_currentPathId);
                }
            }
        }
    }
}

void ResourcePage::displayResources(const QList<ResourceInfo> &resources)
{
    _resourceList->clear();

    if (resources.isEmpty()) {
        QListWidgetItem *emptyItem = new QListWidgetItem();
        emptyItem->setText("暂无资源推荐\n\n点击上方\"生成资源推荐\"按钮，AI将根据学习路径为您推荐优质学习资料");
        emptyItem->setTextAlignment(Qt::AlignCenter);
        emptyItem->setFlags(Qt::NoItemFlags);
        QFont font = emptyItem->font();
        font.setPointSize(11);
        font.setItalic(true);
        emptyItem->setFont(font);
        _resourceList->addItem(emptyItem);
        return;
    }

    for (const ResourceInfo &info : resources) {
        ResourceItem *item = new ResourceItem(info);
        connect(item, &ResourceItem::itemClicked, this, &ResourcePage::onResourceItemClicked);

        QListWidgetItem *listItem = new QListWidgetItem();
        listItem->setSizeHint(item->sizeHint());
        _resourceList->addItem(listItem);
        _resourceList->setItemWidget(listItem, item);
    }

    _statusLabel->setText(QString("共找到 %1 条资源推荐").arg(resources.size()));
    _statusLabel->show();
}

void ResourcePage::openUrl(const QString &url)
{
    if (url.isEmpty()) {
        QMessageBox::warning(this, "提示", "链接为空");
        return;
    }

    bool success = QDesktopServices::openUrl(QUrl(url));
    if (!success) {
        QMessageBox::warning(this, "错误", "无法打开链接: " + url);
    }
}

void ResourcePage::showLoadingState(bool loading)
{
    _generateBtn->setEnabled(!loading);
    _refreshBtn->setEnabled(!loading);
    _pathCombo->setEnabled(!loading);

    if (loading) {
        _statusLabel->setText("正在生成资源推荐，请稍候...");
        _statusLabel->show();
    } else {
        _statusLabel->hide();
    }
}

void ResourcePage::updateGenerateButtonState()
{
    _generateBtn->setEnabled(_currentPathId > 0);
}
