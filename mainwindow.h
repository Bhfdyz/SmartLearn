#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(const QString &username = "", QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onMenuClicked(int index);      // 菜单点击事件
    void onLogoutClicked();             // 退出登录
    void onKnowledgeClicked();          // 打开知识库填写

private:
    Ui::MainWindow *ui;
    QString _username;

    // UI 组件
    QListWidget *_menuList;             // 侧边栏菜单列表
    QStackedWidget *_stackedWidget;     // 内容区域堆栈窗口
    QLabel *_usernameLabel;             // 用户名标签
    QLabel *_welcomeLabel;              // 欢迎标签（首页）
    QPushButton *_logoutBtn;            // 退出按钮
    QPushButton *_editKnowledgeBtn;     // 修改知识库按钮

    // 知识库页面控件
    QListWidget *_knowledgeListWidget;  // 知识点列表
    QLabel *_learningGoalLabel;         // 学习目标标签

    // 页面
    QWidget *_homePage;                 // 首页
    QWidget *_knowledgePage;            // 知识库页面
    QWidget *_aiChatPage;               // AI对话页面
    QWidget *_pathPage;                 // 学习路径页面
    QWidget *_resourcePage;             // 学习资源页面

    void setupUI();                     // 设置UI布局
    void createHomePage();              // 创建首页
    void createKnowledgePage();         // 创建知识库页面
    void createAIChatPage();            // 创建AI对话页面
    void createPathPage();              // 创建学习路径页面
    void createResourcePage();          // 创建学习资源页面
    void refreshKnowledgePage();        // 刷新知识库页面显示

    QWidget* createFeatureCard(const QString &icon, const QString &title, const QString &desc);  // 创建功能卡片
};

#endif // MAINWINDOW_H
