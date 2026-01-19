#ifndef RESOURCEPAGE_H
#define RESOURCEPAGE_H

#include <QWidget>
#include <QList>
#include <QJsonObject>
#include <QJsonArray>
#include <QComboBox>
#include <QPushButton>
#include <QListWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>

// 学习资源信息结构
struct ResourceInfo {
    int         id;
    int         path_id;
    int         stage_order;
    QString     stage_name;
    QString     title;
    QString     url;
    QString     source;
    QString     description;
    int         difficulty;
};

// 学习路径简略信息
struct PathSimpleInfo {
    int path_id;
    QString path_name;
    QString learning_goal;
};

// 前向声明
class ResourceItem;

class ResourcePage : public QWidget
{
    Q_OBJECT

public:
    explicit ResourcePage(const QString &username, QWidget *parent = nullptr);
    void loadPathList();          // 加载学习路径列表
    void refreshResources();      // 刷新资源列表

private slots:
    void onPathChanged(int index);            // 切换路径
    void onGenerateClicked();                 // 生成资源推荐
    void onRefreshClicked();                  // 刷新按钮
    void onResourceItemClicked(const QString &url);  // 点击资源打开链接
    void SlotReadFromServer();                // 接收服务器响应

private:
    void setupUI();
    void requestGenerateResources(int path_id);   // 请求生成资源
    void requestGetResources(int path_id);        // 请求获取资源
    void requestGetPathList();                    // 请求获取路径列表
    void displayResources(const QList<ResourceInfo> &resources);  // 显示资源列表
    void openUrl(const QString &url);             // 打开网页链接
    void showLoadingState(bool loading);          // 显示/隐藏加载状态
    void updateGenerateButtonState();             // 更新生成按钮状态

    QString _username;

    // 当前选中的路径
    int _currentPathId;
    QString _currentPathName;

    // 数据存储
    QList<PathSimpleInfo> _pathList;        // 学习路径列表
    QList<ResourceInfo> _resources;         // 资源列表

    // UI组件
    QLabel *_titleLabel;                    // 页面标题
    QComboBox *_pathCombo;                  // 路径选择下拉框
    QPushButton *_generateBtn;              // 生成推荐按钮
    QPushButton *_refreshBtn;               // 刷新按钮
    QListWidget *_resourceList;             // 资源列表
    QLabel *_statusLabel;                   // 状态提示标签
};

#endif // RESOURCEPAGE_H
