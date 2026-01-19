#include "learningpathpage.h"
#include "stageitem.h"
#include "connectmanager.h"
#include "config.h"

#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMessageBox>
#include <QDebug>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QComboBox>
#include <QDialog>
#include <QProgressBar>
#include <QMenu>
#include <QAction>

LearningPathPage::LearningPathPage(const QString &username, QWidget *parent)
    : QWidget(parent)
    , _username(username)
    , _showingDetail(false)
    , _currentPathId(0)
{
    setupUI();
    loadPathList();
}

void LearningPathPage::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ========== 顶部工具栏 ==========
    QWidget *topBar = new QWidget(this);
    topBar->setStyleSheet("background-color: #f8f9fa; border-bottom: 1px solid #e0e0e0;");
    QHBoxLayout *topBarLayout = new QHBoxLayout(topBar);
    topBarLayout->setContentsMargins(20, 15, 20, 15);

    QLabel *titleLabel = new QLabel("📚 学习路径", topBar);
    titleLabel->setStyleSheet("color: #2c3e50; font-size: 18px; font-weight: bold;");
    topBarLayout->addWidget(titleLabel);

    topBarLayout->addStretch();

    mainLayout->addWidget(topBar);

    // ========== 内容区域（使用QStackedWidget的替代方案） ==========
    // 创建列表视图
    _listView = new QWidget(this);
    _listLayout = new QVBoxLayout(_listView);
    _listLayout->setContentsMargins(20, 20, 20, 20);
    _listLayout->setSpacing(15);

    // 生成路径按钮
    _generatePathBtn = new QPushButton("✨ 生成新学习路径", _listView);
    _generatePathBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #3498db;
            color: white;
            border: none;
            padding: 12px 20px;
            border-radius: 8px;
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
    _generatePathBtn->setFixedHeight(45);
    connect(_generatePathBtn, &QPushButton::clicked, this, &LearningPathPage::onGeneratePathClicked);
    _listLayout->addWidget(_generatePathBtn);

    // 路径列表标题
    QLabel *listTitle = new QLabel("我的学习路径", _listView);
    listTitle->setStyleSheet("color: #7f8c8d; font-size: 14px; font-weight: bold; padding: 5px;");
    _listLayout->addWidget(listTitle);

    // 路径列表
    _pathListWidget = new QListWidget(_listView);
    _pathListWidget->setStyleSheet(R"(
        QListWidget {
            border: none;
            background-color: transparent;
            padding: 5px;
        }
        QListWidget::item {
            border: 1px solid #e0e0e0;
            border-radius: 10px;
            padding: 15px;
            margin: 8px 0px;
            background-color: white;
            color: #2c3e50;
            font-size: 14px;
        }
        QListWidget::item:hover {
            background-color: #f8f9fa;
            border: 1px solid #3498db;
        }
        QListWidget::item:selected {
            background-color: #e3f2fd;
            border: 1px solid #3498db;
        }
    )");

    // 连接双击事件到详情查看
    connect(_pathListWidget, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        int path_id = item->data(Qt::UserRole).toInt();
        showDetailView(path_id);
    });

    // 启用右键菜单
    _pathListWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(_pathListWidget, &QListWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        QListWidgetItem *item = _pathListWidget->itemAt(pos);
        if (item) {
            QMenu menu(this);
            QAction *viewAction = menu.addAction("📖 查看详情");
            menu.addSeparator();
            QAction *deleteAction = menu.addAction("🗑️ 删除路径");

            connect(viewAction, &QAction::triggered, this, [this, item]() {
                int path_id = item->data(Qt::UserRole).toInt();
                showDetailView(path_id);
            });

            connect(deleteAction, &QAction::triggered, this, [this, item]() {
                int path_id = item->data(Qt::UserRole).toInt();
                onDeletePathClicked(path_id);
            });

            menu.exec(_pathListWidget->mapToGlobal(pos));
        }
    });

    _listLayout->addWidget(_pathListWidget, 1);

    mainLayout->addWidget(_listView);

    // ========== 详情视图（初始隐藏）==========
    _detailView = new QWidget(this);
    _detailLayout = new QVBoxLayout(_detailView);
    _detailLayout->setContentsMargins(20, 20, 20, 20);
    _detailLayout->setSpacing(15);

    // 顶部按钮栏
    QWidget *topButtonWidget = new QWidget(_detailView);
    QHBoxLayout *topButtonLayout = new QHBoxLayout(topButtonWidget);
    topButtonLayout->setContentsMargins(0, 0, 0, 0);
    topButtonLayout->setSpacing(10);

    // 返回按钮
    _backBtn = new QPushButton("← 返回列表", _detailView);
    _backBtn->setStyleSheet(R"(
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
    )");
    connect(_backBtn, &QPushButton::clicked, this, &LearningPathPage::onBackToListClicked);
    topButtonLayout->addWidget(_backBtn);

    topButtonLayout->addStretch();

    // 删除按钮
    _deletePathBtn = new QPushButton("🗑️ 删除路径", _detailView);
    _deletePathBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #e74c3c;
            color: white;
            border: none;
            padding: 8px 16px;
            border-radius: 6px;
            font-size: 13px;
        }
        QPushButton:hover {
            background-color: #c0392b;
        }
    )");
    connect(_deletePathBtn, &QPushButton::clicked, this, [this]() {
        onDeletePathClicked(_currentPathId);
    });
    topButtonLayout->addWidget(_deletePathBtn);

    _detailLayout->addWidget(topButtonWidget);

    // 路径标题
    _pathTitleLabel = new QLabel(_detailView);
    _pathTitleLabel->setStyleSheet("color: #2c3e50; font-size: 20px; font-weight: bold; padding: 10px 0;");
    _pathTitleLabel->setWordWrap(true);
    _detailLayout->addWidget(_pathTitleLabel);

    // 进度区域
    QWidget *progressWidget = new QWidget(_detailView);
    progressWidget->setStyleSheet("background-color: #f8f9fa; border-radius: 8px; padding: 10px;");
    QHBoxLayout *progressLayout = new QHBoxLayout(progressWidget);

    _pathProgressLabel = new QLabel("进度: 0%", progressWidget);
    _pathProgressLabel->setStyleSheet("color: #2c3e50; font-size: 13px;");
    progressLayout->addWidget(_pathProgressLabel);

    _progressBar = new QProgressBar(progressWidget);
    _progressBar->setRange(0, 100);
    _progressBar->setValue(0);
    _progressBar->setStyleSheet(R"(
        QProgressBar {
            border: 1px solid #ddd;
            border-radius: 4px;
            background-color: white;
            text-align: center;
            height: 20px;
        }
        QProgressBar::chunk {
            background-color: #27ae60;
            border-radius: 3px;
        }
    )");
    progressLayout->addWidget(_progressBar, 1);

    _detailLayout->addWidget(progressWidget);

    // 阶段列表标题
    QLabel *stageTitle = new QLabel("学习阶段", _detailView);
    stageTitle->setStyleSheet("color: #7f8c8d; font-size: 14px; font-weight: bold; padding: 5px;");
    _detailLayout->addWidget(stageTitle);

    // 阶段列表
    _stageListWidget = new QListWidget(_detailView);
    _stageListWidget->setStyleSheet(R"(
        QListWidget {
            border: none;
            background-color: transparent;
            padding: 5px;
        }
        QListWidget::item {
            border: 1px solid #e0e0e0;
            border-radius: 8px;
            padding: 5px;  // 减少从 12px 到 5px，避免内容被裁剪
            margin: 6px 0px;
            background-color: white;
            color: #2c3e50;
            font-size: 13px;
        }
        QListWidget::item:hover {
            background-color: #f8f9fa;
        }
    )");

    // 不再需要连接itemClicked和itemDoubleClicked，改用StageItem的信号机制

    _detailLayout->addWidget(_stageListWidget, 1);

    _detailView->hide();
    mainLayout->addWidget(_detailView);

    // 初始显示列表视图
    showListView();
}

void LearningPathPage::showListView()
{
    _showingDetail = false;
    _listView->show();
    _detailView->hide();
}

void LearningPathPage::showDetailView(int path_id)
{
    _showingDetail = true;
    _currentPathId = path_id;
    _listView->hide();
    _detailView->show();
    loadPathDetail(path_id);
}

void LearningPathPage::onBackToListClicked()
{
    showListView();
    loadPathList();  // 刷新列表
}

QString LearningPathPage::getStatusText(int status)
{
    switch (status) {
        case 0: return "未开始";
        case 1: return "进行中";
        case 2: return "已完成";
        default: return "未知";
    }
}

// ========== 网络通信 ==========

void LearningPathPage::loadPathList()
{
    ConnectManager &manager = ConnectManager::getInstance();
    QTcpSocket *client = manager.getSocket();

    if (client->state() != QAbstractSocket::ConnectedState) {
        client->abort();
        client->connectToHost(HOSTNAME, PORT);
        if (!client->waitForConnected(3000)) {
            qDebug() << "LearningPathPage: 连接失败";
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
        qDebug() << "LearningPathPage: 收到路径列表响应" << responseData.left(200);

        QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);
        if (!responseDoc.isNull() && responseDoc.isObject()) {
            QJsonObject responseJson = responseDoc.object();

            if (responseJson["status"].toString() == "success") {
                _paths.clear();
                _pathListWidget->clear();

                QJsonArray pathArray = responseJson["paths"].toArray();
                for (const QJsonValue &value : pathArray) {
                    QJsonObject pathObj = value.toObject();
                    LearningPathInfo info;
                    info.path_id = pathObj["path_id"].toInt();
                    info.path_name = pathObj["path_name"].toString();
                    info.learning_goal = pathObj["learning_goal"].toString();
                    info.status = pathObj["status"].toInt();
                    info.progress = pathObj["progress"].toDouble();
                    info.created_at = pathObj["created_at"].toString();
                    _paths.append(info);

                    // 添加到列表
                    QListWidgetItem *item = new QListWidgetItem();
                    QString itemText = "📖 " + info.path_name + "\n"
                                       + "目标: " + info.learning_goal + "\n"
                                       + "状态: " + getStatusText(info.status)
                                       + " | 进度: " + QString::number(info.progress * 100, 'f', 0) + "%";
                    item->setText(itemText);
                    item->setData(Qt::UserRole, info.path_id);
                    _pathListWidget->addItem(item);
                }

                qDebug() << "路径列表加载成功，共" << _paths.size() << "条路径";
            }
        }
    }
}

void LearningPathPage::loadPathDetail(int path_id)
{
    ConnectManager &manager = ConnectManager::getInstance();
    QTcpSocket *client = manager.getSocket();

    if (client->state() != QAbstractSocket::ConnectedState) {
        client->abort();
        client->connectToHost(HOSTNAME, PORT);
        if (!client->waitForConnected(3000)) {
            qDebug() << "LearningPathPage: 连接失败";
            return;
        }
    }

    disconnect(client, &QTcpSocket::readyRead, nullptr, nullptr);

    QJsonObject json;
    json["type"] = GetLearningPathDetailType;
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

            if (responseJson["status"].toString() == "success") {
                // 更新标题
                _pathTitleLabel->setText(responseJson["path_name"].toString());

                // 更新进度 (修复: 使用path_status而不是status)
                double progress = responseJson["progress"].toDouble();
                int pathStatus = responseJson["path_status"].toInt();
                _progressBar->setValue(static_cast<int>(progress * 100));
                _pathProgressLabel->setText("状态: " + getStatusText(pathStatus) + " | 进度: " + QString::number(progress * 100, 'f', 0) + "%");

                // 验证path_data存在
                if (!responseJson.contains("path_data") || responseJson["path_data"].isNull()) {
                    qDebug() << "错误: 响应中不包含path_data";
                    QMessageBox::warning(this, "数据错误", "学习路径数据不完整，请重新生成路径");
                    onBackToListClicked();
                    return;
                }

                // 解析路径数据
                QJsonObject pathData = responseJson["path_data"].toObject();
                qDebug() << "path_data内容:" << QJsonDocument(pathData).toJson(QJsonDocument::Compact);

                if (!pathData.contains("stages")) {
                    qDebug() << "错误: path_data中没有stages字段";
                    QMessageBox::warning(this, "数据错误", "该学习路径没有包含阶段数据，请重新生成路径");
                    onBackToListClicked();
                    return;
                }

                _stages.clear();
                QJsonArray stagesArray = pathData["stages"].toArray();

                if (stagesArray.isEmpty()) {
                    qDebug() << "警告: stages数组为空";
                }

                for (const QJsonValue &stageValue : stagesArray) {
                    QJsonObject stageObj = stageValue.toObject();
                    PathStage stage;
                    stage.stage_order = stageObj["stage_order"].toInt();
                    stage.stage_name = stageObj["stage_name"].toString();
                    stage.stage_description = stageObj["stage_description"].toString();
                    stage.estimated_hours = stageObj["estimated_hours"].toInt();
                    stage.expanded = false;
                    stage.completed = false;  // 初始化为未完成

                    QJsonArray stepsArray = stageObj["steps"].toArray();
                    for (const QJsonValue &stepValue : stepsArray) {
                        stage.steps.append(stepValue.toObject());
                    }
                    _stages.append(stage);
                }

                // 使用refreshStageList显示阶段列表
                refreshStageList();

                qDebug() << "路径详情加载成功，共" << _stages.size() << "个阶段";
            } else {
                // 处理错误响应
                QString errorMsg = responseJson["message"].toString();
                qDebug() << "获取路径详情失败:" << errorMsg;
                QMessageBox::warning(this, "加载失败", "无法加载学习路径详情: " + errorMsg);
                onBackToListClicked();
            }
        } else {
            qDebug() << "解析响应JSON失败";
            QMessageBox::warning(this, "解析错误", "服务器响应格式错误");
            onBackToListClicked();
        }
    } else {
        qDebug() << "等待响应超时";
        QMessageBox::warning(this, "网络错误", "连接服务器超时，请检查网络连接");
        onBackToListClicked();
    }
}

void LearningPathPage::refreshPaths()
{
    if (_showingDetail) {
        loadPathDetail(_currentPathId);
    } else {
        loadPathList();
    }
}

void LearningPathPage::onGeneratePathClicked()
{
    // 弹出对话框让用户输入信息
    QDialog dialog(this);
    dialog.setWindowTitle("生成学习路径");
    dialog.setFixedSize(450, 350);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    layout->setSpacing(15);

    // 路径名称
    layout->addWidget(new QLabel("路径名称:"));
    QLineEdit *nameEdit = new QLineEdit(&dialog);
    nameEdit->setPlaceholderText("例如：Java后端开发学习路径");
    layout->addWidget(nameEdit);

    // 学习目标
    layout->addWidget(new QLabel("学习目标:"));
    QTextEdit *goalEdit = new QTextEdit(&dialog);
    goalEdit->setPlaceholderText("描述你的学习目标，例如：掌握Java后端开发核心技能");
    goalEdit->setMaximumHeight(80);
    layout->addWidget(goalEdit);

    // 学习时长
    layout->addWidget(new QLabel("预期学习时长:"));
    QComboBox *timeCombo = new QComboBox(&dialog);
    timeCombo->addItem("1个月");
    timeCombo->addItem("3个月");
    timeCombo->addItem("6个月");
    timeCombo->addItem("1年");
    timeCombo->setCurrentIndex(1);
    layout->addWidget(timeCombo);

    // 难度选择
    layout->addWidget(new QLabel("期望难度:"));
    QComboBox *difficultyCombo = new QComboBox(&dialog);
    difficultyCombo->addItem("入门");
    difficultyCombo->addItem("中等");
    difficultyCombo->addItem("进阶");
    difficultyCombo->setCurrentIndex(1);
    layout->addWidget(difficultyCombo);

    layout->addStretch();

    // 按钮
    QHBoxLayout *btnLayout = new QHBoxLayout();
    QPushButton *cancelBtn = new QPushButton("取消", &dialog);
    QPushButton *okBtn = new QPushButton("生成路径", &dialog);
    okBtn->setStyleSheet("background-color: #3498db; color: white; padding: 8px 20px; border-radius: 4px;");
    btnLayout->addWidget(cancelBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(okBtn);
    layout->addLayout(btnLayout);

    connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(okBtn, &QPushButton::clicked, &dialog, &QDialog::accept);

    if (dialog.exec() == QDialog::Accepted) {
        QString pathName = nameEdit->text().trimmed();
        QString learningGoal = goalEdit->toPlainText().trimmed();
        QString timePref = timeCombo->currentText();
        QString difficulty = difficultyCombo->currentText();

        if (pathName.isEmpty() || learningGoal.isEmpty()) {
            QMessageBox::warning(this, "提示", "请填写完整的路径名称和学习目标");
            return;
        }

        sendGeneratePathRequest(pathName, learningGoal, timePref, difficulty);
    }
}

void LearningPathPage::sendGeneratePathRequest(const QString &path_name, const QString &learning_goal,
                                                const QString &time_preference, const QString &difficulty)
{
    ConnectManager &manager = ConnectManager::getInstance();
    QTcpSocket *client = manager.getSocket();

    if (client->state() != QAbstractSocket::ConnectedState) {
        client->abort();
        client->connectToHost(HOSTNAME, PORT);
        if (!client->waitForConnected(3000)) {
            QMessageBox::warning(this, "连接错误", "无法连接到服务器");
            return;
        }
    }

    disconnect(client, &QTcpSocket::readyRead, nullptr, nullptr);

    // 第一步：先获取用户的知识库
    QJsonObject getKnowledgeJson;
    getKnowledgeJson["type"] = GetKnowledgeType;
    getKnowledgeJson["username"] = _username;

    QJsonDocument getKnowledgeDoc(getKnowledgeJson);
    client->write(getKnowledgeDoc.toJson());
    client->flush();

    QJsonArray knowledgeArray;
    if (client->waitForReadyRead(3000)) {
        QByteArray knowledgeResponse = client->readAll();
        QJsonDocument knowledgeDoc = QJsonDocument::fromJson(knowledgeResponse);
        if (!knowledgeDoc.isNull() && knowledgeDoc.isObject()) {
            QJsonObject knowledgeObj = knowledgeDoc.object();
            if (knowledgeObj["status"] == "success" || knowledgeObj["status"].toString() == "success") {
                knowledgeArray = knowledgeObj["knowledge_points"].toArray();
                qDebug() << "获取到用户知识点:" << knowledgeArray.size() << "个";
            }
        }
    }

    // 第二步：发送生成路径请求（带上知识库数据）
    QJsonObject json;
    json["type"] = GenerateLearningPathType;
    json["username"] = _username;
    json["path_name"] = path_name;
    json["learning_goal"] = learning_goal;
    json["time_preference"] = time_preference;
    json["difficulty"] = difficulty;
    json["current_knowledge"] = knowledgeArray;  // 使用实际获取到的知识库

    QJsonDocument doc(json);
    client->write(doc.toJson());
    client->flush();

    // 显示等待提示
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("生成中");
    msgBox.setText("AI正在为您规划学习路径，请稍候...");
    msgBox.setStandardButtons(QMessageBox::NoButton);
    msgBox.show();

    // 等待响应（150秒超时，与服务端一致）
    if (client->waitForReadyRead(150000)) {
        QByteArray responseData = client->readAll();
        msgBox.close();

        QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);
        if (!responseDoc.isNull() && responseDoc.isObject()) {
            QJsonObject responseJson = responseDoc.object();

            if (responseJson["status"].toString() == "success") {
                QMessageBox::information(this, "成功", "学习路径生成成功！");
                loadPathList();
            } else {
                QString errorMsg = responseJson["message"].toString();
                QMessageBox::warning(this, "生成失败", "无法生成学习路径: " + errorMsg);
            }
        }
    } else {
        msgBox.close();
        QMessageBox::warning(this, "超时", "请求超时，请稍后重试");
    }
}

void LearningPathPage::onViewPathClicked(int path_id)
{
    showDetailView(path_id);
}

void LearningPathPage::onDeletePathClicked(int path_id)
{
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "确认删除", "确定要删除这条学习路径吗？此操作无法撤销。",
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        sendDeletePathRequest(path_id);
    }
}

void LearningPathPage::onStageCompletionChanged(int index, bool completed)
{
    if (index < 0 || index >= _stages.size()) return;

    // 更新本地阶段状态
    _stages[index].completed = completed;

    // 当阶段标记为完成时，同步更新所有步骤状态为完成（不重新创建UI）
    if (completed) {
        for (int i = 0; i < _stages[index].steps.size(); ++i) {
            QJsonObject stepObj = _stages[index].steps[i];
            stepObj["completed"] = true;
            _stages[index].steps[i] = stepObj;
        }

        // 使用新方法更新所有步骤状态，避免UI重复
        if (_stageItems[index]) {
            _stageItems[index]->setAllStepsCompleted(true);
        }
    }

    // 计算已完成数量
    int completedCount = 0;
    for (const PathStage &stage : _stages) {
        if (stage.completed) completedCount++;
    }

    // 发送更新请求并等待响应
    sendUpdateProgressRequest(_currentPathId, _stages[index].stage_order,
                              completed, completedCount, _stages.size());
}

void LearningPathPage::onStageExpansionChanged(int index, bool expanded)
{
    if (index < 0 || index >= _stages.size()) return;

    // 更新本地状态
    _stages[index].expanded = expanded;

    // 更新该StageItem的显示
    if (_stageItems[index]) {
        _stageItems[index]->setExpanded(expanded);

        // 更新列表项的大小以适应展开/收起后的高度变化
        QListWidgetItem *listItem = _stageListWidget->item(index);
        if (listItem) {
            listItem->setSizeHint(_stageItems[index]->sizeHint());
        }
    }
}

void LearningPathPage::refreshStageList()
{
    _stageListWidget->clear();
    _stageItems.clear();

    for (int i = 0; i < _stages.size(); ++i) {
        StageItem *item = new StageItem(_stages[i], i, this);

        connect(item, &StageItem::completionChanged,
                this, &LearningPathPage::onStageCompletionChanged);
        connect(item, &StageItem::expansionChanged,
                this, &LearningPathPage::onStageExpansionChanged);
        connect(item, &StageItem::stepCompletedChanged,
                this, &LearningPathPage::onStepCompletedChanged);

        QListWidgetItem *listItem = new QListWidgetItem();
        listItem->setSizeHint(item->sizeHint());
        _stageListWidget->addItem(listItem);
        _stageListWidget->setItemWidget(listItem, item);
        // 强制更新几何信息，确保大小正确应用
        listItem->setSizeHint(item->sizeHint());

        _stageItems.append(item);
    }
}

void LearningPathPage::sendDeletePathRequest(int path_id)
{
    ConnectManager &manager = ConnectManager::getInstance();
    QTcpSocket *client = manager.getSocket();

    if (client->state() != QAbstractSocket::ConnectedState) {
        client->abort();
        client->connectToHost(HOSTNAME, PORT);
        if (!client->waitForConnected(3000)) {
            QMessageBox::warning(this, "连接错误", "无法连接到服务器");
            return;
        }
    }

    disconnect(client, &QTcpSocket::readyRead, nullptr, nullptr);

    QJsonObject json;
    json["type"] = DeleteLearningPathType;
    json["username"] = _username;
    json["path_id"] = path_id;

    QJsonDocument doc(json);
    client->write(doc.toJson());
    client->flush();

    if (client->waitForReadyRead(3000)) {
        QByteArray responseData = client->readAll();

        QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);
        if (!responseDoc.isNull() && responseDoc.isObject()) {
            QJsonObject responseJson = responseDoc.object();

            if (responseJson["status"].toString() == "success") {
                QMessageBox::information(this, "成功", "学习路径已删除");
                loadPathList();
            } else {
                QMessageBox::warning(this, "删除失败", responseJson["message"].toString());
            }
        }
    }
}

void LearningPathPage::sendUpdateProgressRequest(int path_id, int stage_order, bool completed, int completed_count, int total_count)
{
    ConnectManager &manager = ConnectManager::getInstance();
    QTcpSocket *client = manager.getSocket();

    if (client->state() != QAbstractSocket::ConnectedState) {
        client->abort();
        client->connectToHost(HOSTNAME, PORT);
        if (!client->waitForConnected(3000)) {
            qDebug() << "更新进度: 连接失败";
            return;
        }
    }

    disconnect(client, &QTcpSocket::readyRead, nullptr, nullptr);

    QJsonObject json;
    json["type"] = UpdatePathProgressType;
    json["username"] = _username;
    json["path_id"] = path_id;
    json["stage_order"] = stage_order;
    json["completed"] = completed;
    json["completed_count"] = completed_count;
    json["total_count"] = total_count;

    QJsonDocument doc(json);
    client->write(doc.toJson());
    client->flush();

    // 等待服务器响应并更新进度显示
    if (client->waitForReadyRead(3000)) {
        QByteArray responseData = client->readAll();
        QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);

        if (!responseDoc.isNull() && responseDoc.isObject()) {
            QJsonObject responseJson = responseDoc.object();

            if (responseJson["status"].toString() == "success") {
                // 使用服务器返回的进度和状态值
                double newProgress = responseJson["progress"].toDouble();
                int newStatus = responseJson["path_status"].toInt();

                // 获取步骤数信息
                int completedSteps = responseJson["completed_steps"].toInt();
                int totalSteps = responseJson["total_steps"].toInt();

                // 更新进度条和状态标签
                _progressBar->setValue(static_cast<int>(newProgress * 100));
                _pathProgressLabel->setText("状态: " + getStatusText(newStatus) +
                                           " | 进度: " + QString::number(completedSteps) + "/" + QString::number(totalSteps) +
                                           " (" + QString::number(newProgress * 100, 'f', 0) + "%)");

                qDebug() << "进度更新成功: progress=" << newProgress << "status=" << newStatus;
            } else {
                qDebug() << "进度更新失败: " << responseJson["message"].toString();
            }
        }
    } else {
        qDebug() << "等待进度更新响应超时";
    }

    qDebug() << "发送进度更新请求: path_id=" << path_id << "stage_order=" << stage_order << "completed=" << completed;
}

void LearningPathPage::onStepCompletedChanged(int stageIndex, int stepIndex, bool completed)
{
    if (stageIndex < 0 || stageIndex >= _stages.size()) return;

    // 获取阶段和步骤信息
    PathStage &stage = _stages[stageIndex];
    if (stepIndex < 0 || stepIndex >= stage.steps.size()) return;

    const QJsonObject &step = stage.steps[stepIndex];
    int step_order = step["step_order"].toInt();

    // 发送步骤进度更新请求
    sendUpdateStepProgressRequest(_currentPathId, stage.stage_order, step_order, completed);
}

void LearningPathPage::sendUpdateStepProgressRequest(int path_id, int stage_order, int step_order, bool completed)
{
    ConnectManager &manager = ConnectManager::getInstance();
    QTcpSocket *client = manager.getSocket();

    if (client->state() != QAbstractSocket::ConnectedState) {
        client->abort();
        client->connectToHost(HOSTNAME, PORT);
        if (!client->waitForConnected(3000)) {
            qDebug() << "更新步骤进度: 连接失败";
            return;
        }
    }

    disconnect(client, &QTcpSocket::readyRead, nullptr, nullptr);

    QJsonObject json;
    json["type"] = UpdateStepProgressType;
    json["username"] = _username;
    json["path_id"] = path_id;
    json["stage_order"] = stage_order;
    json["step_order"] = step_order;
    json["completed"] = completed;

    QJsonDocument doc(json);
    client->write(doc.toJson());
    client->flush();

    // 等待服务器响应并更新进度显示
    if (client->waitForReadyRead(3000)) {
        QByteArray responseData = client->readAll();
        QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);

        if (!responseDoc.isNull() && responseDoc.isObject()) {
            QJsonObject responseJson = responseDoc.object();

            if (responseJson["status"].toString() == "success") {
                // 使用服务器返回的进度和状态值
                double newProgress = responseJson["progress"].toDouble();
                int newStatus = responseJson["path_status"].toInt();
                int completedSteps = responseJson["completed_steps"].toInt();
                int totalSteps = responseJson["total_steps"].toInt();

                // 更新进度条和状态标签
                _progressBar->setValue(static_cast<int>(newProgress * 100));
                _pathProgressLabel->setText("状态: " + getStatusText(newStatus) +
                                           " | 进度: " + QString::number(completedSteps) + "/" + QString::number(totalSteps) +
                                           " (" + QString::number(newProgress * 100, 'f', 0) + "%)");

                qDebug() << "步骤进度更新成功: progress=" << newProgress << "status=" << newStatus;
            } else {
                qDebug() << "步骤进度更新失败: " << responseJson["message"].toString();
            }
        }
    } else {
        qDebug() << "等待步骤进度更新响应超时";
    }

    qDebug() << "发送步骤进度更新请求: path_id=" << path_id << "stage_order=" << stage_order << "step_order=" << step_order << "completed=" << completed;
}

void LearningPathPage::SlotReadFromServer()
{
    // 保留接口，用于处理异步响应
}
