#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "knowledgedialog.h"
#include "connectmanager.h"
#include "config.h"
#include "aichatpage.h"
#include "learningpathpage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QStackedWidget>
#include <QWidget>
#include <QMessageBox>
#include <QDebug>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

MainWindow::MainWindow(const QString &username, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , _username(username)
{
    ui->setupUi(this);

    setWindowTitle("SmartLearn - 智能学习路径规划系统");
    resize(1400, 850);

    setupUI();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupUI()
{
    // 创建中心部件
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    // 主布局
    QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ========== 左侧边栏 ==========
    QWidget *sidebar = new QWidget(this);
    sidebar->setFixedWidth(250);
    sidebar->setStyleSheet(R"(
        QWidget {
            background-color: #2c3e50;
        }
    )");

    QVBoxLayout *sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(0, 0, 0, 0);
    sidebarLayout->setSpacing(0);

    // Logo区域
    QLabel *logoLabel = new QLabel("SmartLearn", sidebar);
    logoLabel->setAlignment(Qt::AlignCenter);
    logoLabel->setFixedHeight(80);
    logoLabel->setStyleSheet(R"(
        QLabel {
            background-color: #1a252f;
            color: #3498db;
            font-size: 24px;
            font-weight: bold;
            padding: 20px;
        }
    )");
    sidebarLayout->addWidget(logoLabel);

    // 菜单列表
    _menuList = new QListWidget(sidebar);
    _menuList->setStyleSheet(R"(
        QListWidget {
            background-color: #2c3e50;
            border: none;
            outline: none;
        }
        QListWidget::item {
            color: #ecf0f1;
            padding: 15px 20px;
            border: none;
        }
        QListWidget::item:hover {
            background-color: #34495e;
        }
        QListWidget::item:selected {
            background-color: #3498db;
            color: white;
        }
    )");
    _menuList->setFocusPolicy(Qt::NoFocus);

    // 添加菜单项
    _menuList->addItem("🏠 首页");
    _menuList->addItem("📚 我的知识库");
    _menuList->addItem("🤖 AI学习助手");
    _menuList->addItem("🗺️ 学习路径");
    _menuList->addItem("📖 学习资源");
    _menuList->addItem("⚙️ 设置");

    sidebarLayout->addWidget(_menuList);

    // 底部用户信息
    QWidget *userInfoWidget = new QWidget(sidebar);
    userInfoWidget->setFixedHeight(100);
    userInfoWidget->setStyleSheet(R"(
        QWidget {
            background-color: #1a252f;
        }
    )");

    QVBoxLayout *userInfoLayout = new QVBoxLayout(userInfoWidget);
    userInfoLayout->setContentsMargins(15, 10, 15, 10);

    _usernameLabel = new QLabel("用户: " + _username, userInfoWidget);
    _usernameLabel->setStyleSheet("color: #ecf0f1; font-size: 14px;");
    userInfoLayout->addWidget(_usernameLabel);

    _logoutBtn = new QPushButton("退出登录", userInfoWidget);
    _logoutBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #e74c3c;
            color: white;
            border: none;
            padding: 8px;
            border-radius: 4px;
            font-size: 13px;
        }
        QPushButton:hover {
            background-color: #c0392b;
        }
    )");
    connect(_logoutBtn, &QPushButton::clicked, this, &MainWindow::onLogoutClicked);
    userInfoLayout->addWidget(_logoutBtn);

    sidebarLayout->addWidget(userInfoWidget);
    sidebarLayout->addStretch();
    mainLayout->addWidget(sidebar);

    // ========== 右侧内容区域 ==========
    QWidget *contentWidget = new QWidget(this);
    contentWidget->setStyleSheet("background-color: #ecf0f1;");

    QVBoxLayout *contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(20, 20, 20, 20);
    contentLayout->setSpacing(15);

    // 顶部标题栏
    QWidget *titleBar = new QWidget(contentWidget);
    titleBar->setFixedHeight(70);
    titleBar->setStyleSheet(R"(
        QWidget {
            background-color: white;
            border-radius: 10px;
        }
    )");

    QHBoxLayout *titleBarLayout = new QHBoxLayout(titleBar);
    titleBarLayout->setContentsMargins(20, 0, 20, 0);

    QLabel *pageTitle = new QLabel("欢迎使用 SmartLearn", titleBar);
    pageTitle->setStyleSheet(R"(
        QLabel {
            font-size: 20px;
            font-weight: bold;
            color: #2c3e50;
        }
    )");
    titleBarLayout->addWidget(pageTitle);
    titleBarLayout->addStretch();

    _editKnowledgeBtn = new QPushButton("修改知识库", titleBar);
    _editKnowledgeBtn->setStyleSheet(R"(
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
    )");
    connect(_editKnowledgeBtn, &QPushButton::clicked, this, &MainWindow::onKnowledgeClicked);
    titleBarLayout->addWidget(_editKnowledgeBtn);

    contentLayout->addWidget(titleBar);

    // 堆栈窗口（用于切换不同页面）
    _stackedWidget = new QStackedWidget(contentWidget);

    // 创建各个页面
    createHomePage();
    createKnowledgePage();
    createAIChatPage();
    createPathPage();
    createResourcePage();

    contentLayout->addWidget(_stackedWidget);

    mainLayout->addWidget(contentWidget, 1);

    // 连接菜单点击信号
    connect(_menuList, &QListWidget::currentRowChanged, this, &MainWindow::onMenuClicked);

    // 默认选中首页
    _menuList->setCurrentRow(0);
}

void MainWindow::createHomePage()
{
    _homePage = new QWidget();
    _homePage->setStyleSheet("background-color: white; border-radius: 10px;");

    QVBoxLayout *layout = new QVBoxLayout(_homePage);
    layout->setContentsMargins(30, 30, 30, 30);
    layout->setSpacing(20);

    // 欢迎标签
    _welcomeLabel = new QLabel("你好，" + _username + "！", _homePage);
    _welcomeLabel->setStyleSheet(R"(
        QLabel {
            font-size: 28px;
            font-weight: bold;
            color: #2c3e50;
        }
    )");
    layout->addWidget(_welcomeLabel);

    QLabel *subtitleLabel = new QLabel("欢迎使用 SmartLearn 智能学习路径规划系统", _homePage);
    subtitleLabel->setStyleSheet(R"(
        QLabel {
            font-size: 16px;
            color: #7f8c8d;
        }
    )");
    layout->addWidget(subtitleLabel);

    layout->addSpacing(30);

    // 功能卡片区域
    QLabel *featuresLabel = new QLabel("快速开始", _homePage);
    featuresLabel->setStyleSheet(R"(
        QLabel {
            font-size: 18px;
            font-weight: bold;
            color: #34495e;
        }
    )");
    layout->addWidget(featuresLabel);

    QHBoxLayout *cardsLayout = new QHBoxLayout();
    cardsLayout->setSpacing(20);

    // 卡片1：知识库
    QWidget *card1 = createFeatureCard("📚", "我的知识库", "查看和管理你已掌握的知识点");
    cardsLayout->addWidget(card1);

    // 卡片2：AI助手
    QWidget *card2 = createFeatureCard("🤖", "AI学习助手", "与AI对话，获取学习建议");
    cardsLayout->addWidget(card2);

    // 卡片3：学习路径
    QWidget *card3 = createFeatureCard("🗺️", "学习路径", "查看个性化学习路径规划");
    cardsLayout->addWidget(card3);

    layout->addLayout(cardsLayout);
    layout->addStretch();

    _stackedWidget->addWidget(_homePage);
}

void MainWindow::createKnowledgePage()
{
    _knowledgePage = new QWidget();
    _knowledgePage->setStyleSheet("background-color: white; border-radius: 10px;");

    QVBoxLayout *layout = new QVBoxLayout(_knowledgePage);
    layout->setContentsMargins(30, 30, 30, 30);
    layout->setSpacing(20);

    // 标题
    QLabel *title = new QLabel("我的知识库", _knowledgePage);
    title->setStyleSheet("font-size: 24px; font-weight: bold; color: #2c3e50;");
    layout->addWidget(title);

    // 学习目标区域
    QLabel *goalTitleLabel = new QLabel("学习目标", _knowledgePage);
    goalTitleLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #34495e;");
    layout->addWidget(goalTitleLabel);

    _learningGoalLabel = new QLabel("暂未设置学习目标", _knowledgePage);
    _learningGoalLabel->setStyleSheet(
        "color: #7f8c8d; font-size: 14px; padding: 10px; "
        "background-color: #f8f9fa; border-radius: 5px;"
    );
    _learningGoalLabel->setWordWrap(true);
    layout->addWidget(_learningGoalLabel);

    // 知识点列表区域
    QLabel *listTitleLabel = new QLabel("已掌握的知识点", _knowledgePage);
    listTitleLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #34495e;");
    layout->addWidget(listTitleLabel);

    _knowledgeListWidget = new QListWidget(_knowledgePage);
    _knowledgeListWidget->setStyleSheet(
        "QListWidget {"
        "   border: 1px solid #ddd;"
        "   border-radius: 8px;"
        "   background-color: #f8f9fa;"
        "   padding: 5px;"
        "}"
        "QListWidget::item {"
        "   padding: 12px;"
        "   margin: 2px;"
        "   border-radius: 5px;"
        "   background-color: white;"
        "   color: #2c3e50;"
        "   font-size: 14px;"
        "}"
        "QListWidget::item:hover {"
        "   background-color: #e3f2fd;"
        "}"
        "QListWidget::item:selected {"
        "   background-color: #2196F3;"
        "   color: white;"
        "}"
    );
    _knowledgeListWidget->setMaximumHeight(300);
    layout->addWidget(_knowledgeListWidget);

    // 提示标签
    QLabel *tip = new QLabel("点击上方「修改知识库」按钮来更新你的知识点", _knowledgePage);
    tip->setStyleSheet("color: #95a5a6; font-size: 12px; font-style: italic;");
    tip->setAlignment(Qt::AlignCenter);
    layout->addWidget(tip);

    layout->addStretch();

    _stackedWidget->addWidget(_knowledgePage);
}

void MainWindow::createAIChatPage()
{
    _aiChatPage = new AIChatPage(_username, this);
    _stackedWidget->addWidget(_aiChatPage);
}

void MainWindow::createPathPage()
{
    _pathPage = new LearningPathPage(_username, this);
    _stackedWidget->addWidget(_pathPage);
}

void MainWindow::createResourcePage()
{
    _resourcePage = new QWidget();
    _resourcePage->setStyleSheet("background-color: white; border-radius: 10px;");

    QVBoxLayout *layout = new QVBoxLayout(_resourcePage);
    layout->setContentsMargins(30, 30, 30, 30);

    QLabel *title = new QLabel("学习资源推荐", _resourcePage);
    title->setStyleSheet("font-size: 24px; font-weight: bold; color: #2c3e50;");
    layout->addWidget(title);

    QLabel *content = new QLabel("学习资源推荐功能开发中...\n\n敬请期待！", _resourcePage);
    content->setStyleSheet("color: #7f8c8d; font-size: 16px;");
    content->setAlignment(Qt::AlignCenter);
    layout->addWidget(content);

    layout->addStretch();

    _stackedWidget->addWidget(_resourcePage);
}

QWidget* MainWindow::createFeatureCard(const QString &icon, const QString &title, const QString &desc)
{
    QWidget *card = new QWidget();
    card->setFixedSize(280, 150);
    card->setStyleSheet(R"(
        QWidget {
            background-color: white;
            border: 2px solid #ecf0f1;
            border-radius: 10px;
        }
        QWidget:hover {
            border: 2px solid #3498db;
            background-color: #f8f9fa;
        }
    )");

    QVBoxLayout *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(20, 20, 20, 20);
    cardLayout->setSpacing(10);

    QLabel *iconLabel = new QLabel(icon, card);
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setStyleSheet("font-size: 48px;");
    cardLayout->addWidget(iconLabel);

    QLabel *titleLabel = new QLabel(title, card);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #2c3e50;");
    cardLayout->addWidget(titleLabel);

    QLabel *descLabel = new QLabel(desc, card);
    descLabel->setAlignment(Qt::AlignCenter);
    descLabel->setWordWrap(true);
    descLabel->setStyleSheet("font-size: 12px; color: #7f8c8d;");
    cardLayout->addWidget(descLabel);

    return card;
}

void MainWindow::onMenuClicked(int index)
{
    _stackedWidget->setCurrentIndex(index);

    // 根据选中的菜单更新标题和按钮
    QString titles[] = {"欢迎使用 SmartLearn", "我的知识库", "AI 学习助手", "学习路径规划", "学习资源推荐", "系统设置"};
    // 这里可以更新顶部标题等

    // 切换到知识库页面时，自动刷新数据
    if (index == 1) {  // "我的知识库" 的索引是 1
        refreshKnowledgePage();
    }

    // 切换到学习路径页面时，自动刷新数据
    if (index == 3) {  // "学习路径" 的索引是 3
        LearningPathPage *pathPage = qobject_cast<LearningPathPage*>(_pathPage);
        if (pathPage) {
            pathPage->refreshPaths();
        }
    }
}

void MainWindow::onLogoutClicked()
{
    if (QMessageBox::question(this, "退出登录", "确定要退出登录吗？") == QMessageBox::Yes) {
        close();  // 关闭主窗口，程序返回到登录界面
    }
}

void MainWindow::onKnowledgeClicked()
{
    // 打开知识库填写对话框
    KnowledgeDialog knowledgeDlg(_username, this);

    // 获取socket并检查连接
    ConnectManager &manager = ConnectManager::getInstance();
    QTcpSocket *client = manager.getSocket();

    // 确保socket已连接
    if (client->state() != QAbstractSocket::ConnectedState) {
        qDebug() << "MainWindow: Socket未连接，尝试重新连接";
        client->connectToHost("127.0.0.1", 8080);
        if (!client->waitForConnected(3000)) {
            QMessageBox::warning(this, "连接错误", "无法连接到服务器");
            return;
        }
    }

    // KnowledgeDialog现在自己管理信号连接
    knowledgeDlg.exec();

    // 对话框关闭后，刷新知识库页面显示
    refreshKnowledgePage();
}

void MainWindow::refreshKnowledgePage()
{
    qDebug() << "=== refreshKnowledgePage 开始 ===";

    // 获取socket连接
    ConnectManager &manager = ConnectManager::getInstance();
    QTcpSocket *client = manager.getSocket();

    // 确保socket已连接
    if (client->state() != QAbstractSocket::ConnectedState) {
        qDebug() << "刷新知识库页面：Socket未连接，尝试重新连接";
        client->abort();
        client->connectToHost("127.0.0.1", 8080);
        if (!client->waitForConnected(3000)) {
            qDebug() << "刷新知识库页面：连接失败";
            return;
        }
    }

    // 断开所有readyRead信号连接，防止数据被其他槽函数消费
    // 使用disconnect()断开sender的所有receiver
    disconnect(client, &QTcpSocket::readyRead, nullptr, nullptr);

    // 构造获取知识库请求
    QJsonObject json;
    json["type"] = GetKnowledgeType;
    json["username"] = _username;

    QJsonDocument doc(json);
    QByteArray data = doc.toJson();

    qDebug() << "刷新知识库：发送请求";

    // 发送请求
    client->write(data);
    client->flush();

    // 等待响应
    if (client->waitForReadyRead(3000)) {
        QByteArray responseData = client->readAll();
        qDebug() << "刷新知识库：收到响应" << responseData;

        QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);
        if (!responseDoc.isNull() && responseDoc.isObject()) {
            QJsonObject responseJson = responseDoc.object();
            QString type = responseJson["type"].toString();
            QString status = responseJson["status"].toString();

            if (type == "KnowledgeResponse" && status == "success") {
                // 更新学习目标
                if (responseJson.contains("learning_goal")) {
                    QString goal = responseJson["learning_goal"].toString();
                    if (goal.isEmpty()) {
                        _learningGoalLabel->setText("暂未设置学习目标");
                    } else {
                        _learningGoalLabel->setText(goal);
                    }
                }

                // 更新知识点列表
                QJsonArray knowledgeArray = responseJson["knowledge_points"].toArray();
                _knowledgeListWidget->clear();

                if (knowledgeArray.isEmpty()) {
                    _knowledgeListWidget->addItem("(暂无知识点)");
                } else {
                    for (const QJsonValue &value : knowledgeArray) {
                        _knowledgeListWidget->addItem(value.toString());
                    }
                }

                qDebug() << "刷新知识库页面成功，共" << knowledgeArray.size() << "个知识点";
            } else {
                qDebug() << "刷新知识库页面失败:" << responseJson["message"].toString();
            }
        } else {
            qDebug() << "刷新知识库页面：响应解析失败";
        }
    } else {
        qDebug() << "刷新知识库页面：等待响应超时";
    }

    qDebug() << "=== refreshKnowledgePage 结束 ===";
}
