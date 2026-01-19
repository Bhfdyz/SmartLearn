#ifndef LEARNINGPATHPAGE_H
#define LEARNINGPATHPAGE_H

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QProgressBar>
#include <QJsonObject>
#include <QVector>

// 前向声明
class StageItem;

// 学习路径信息结构
struct LearningPathInfo {
    int path_id;
    QString path_name;
    QString learning_goal;
    int status;          // 0:未开始 1:进行中 2:已完成
    double progress;     // 0.0-1.0
    QString created_at;
    QString updated_at;
    QString path_data;   // JSON格式的完整路径数据
};

// 路径阶段结构
struct PathStage {
    int stage_order;
    QString stage_name;
    QString stage_description;
    int estimated_hours;
    QList<QJsonObject> steps;
    bool expanded;       // 是否展开显示
    bool completed;      // 阶段是否完成
};

class LearningPathPage : public QWidget
{
    Q_OBJECT

public:
    explicit LearningPathPage(const QString &username, QWidget *parent = nullptr);
    void loadPathList();              // 加载路径列表
    void refreshPaths();              // 刷新路径列表

signals:
    void openKnowledgeDialog();       // 打开知识库对话框信号

private slots:
    void onGeneratePathClicked();     // 生成新路径按钮
    void onViewPathClicked(int path_id);     // 查看路径详情
    void onDeletePathClicked(int path_id);   // 删除路径
    void onBackToListClicked();       // 返回列表
    void onStageCompletionChanged(int index, bool completed);   // 阶段完成状态变化
    void onStageExpansionChanged(int index, bool expanded);     // 阶段展开/收起
    void onStepCompletedChanged(int stageIndex, int stepIndex, bool completed);  // 步骤完成状态变化
    void SlotReadFromServer();        // 接收服务器响应

private:
    QString _username;

    // UI组件 - 列表视图
    QWidget *_listView;
    QVBoxLayout *_listLayout;
    QPushButton *_generatePathBtn;
    QListWidget *_pathListWidget;

    // UI组件 - 详情视图
    QWidget *_detailView;
    QVBoxLayout *_detailLayout;
    QPushButton *_backBtn;
    QPushButton *_deletePathBtn;
    QLabel *_pathTitleLabel;
    QLabel *_pathProgressLabel;
    QProgressBar *_progressBar;
    QListWidget *_stageListWidget;

    // 当前显示状态
    bool _showingDetail;
    int _currentPathId;

    // 数据
    QList<LearningPathInfo> _paths;
    QList<PathStage> _stages;
    QVector<StageItem*> _stageItems;  // 存储StageItem指针以便更新

    // 方法
    void setupUI();
    void showListView();              // 显示列表视图
    void showDetailView(int path_id); // 显示详情视图
    void loadPathDetail(int path_id); // 加载路径详情
    void refreshStageList();          // 刷新阶段列表显示
    void sendGeneratePathRequest(const QString &path_name, const QString &learning_goal,
                                  const QString &time_preference, const QString &difficulty);
    void sendDeletePathRequest(int path_id);
    void sendUpdateProgressRequest(int path_id, int stage_order, bool completed, int completed_count, int total_count);
    void sendUpdateStepProgressRequest(int path_id, int stage_order, int step_order, bool completed);  // 新增：更新步骤进度
    QString getStatusText(int status);  // 获取状态文本
};

#endif // LEARNINGPATHPAGE_H
