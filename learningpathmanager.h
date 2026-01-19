#ifndef LEARNINGPATHMANAGER_H
#define LEARNINGPATHMANAGER_H

#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QList>
#include <QProcess>
#include <QTemporaryFile>

// 学习路径结构
struct LearningPath {
    int path_id;
    int user_id;
    QString path_name;
    QString learning_goal;
    QString path_data;        // JSON格式存储完整路径
    int status;               // 0:未开始 1:进行中 2:已完成
    double progress;          // 0.0-1.0
    QString created_at;
    QString updated_at;
};

// 简化的阶段结构（用于解析JSON）
struct PathStage {
    int stage_order;
    QString stage_name;
    QString stage_description;
    int estimated_hours;
    QList<QJsonObject> steps;
};

// AI生成路径响应结构
struct GeneratePathResponse {
    bool success;
    QString error;
    int path_id;
    QJsonObject path_data;    // AI生成的完整路径JSON
};

class LearningPathManager
{
public:
    static LearningPathManager& getInstance();

    // ========== 路径生成 ==========
    GeneratePathResponse generatePath(int user_id,
                                      const QString &path_name,
                                      const QString &learning_goal,
                                      const QStringList &current_knowledge,
                                      const QString &time_preference = "3个月",
                                      const QString &difficulty = "intermediate");

    // ========== 路径查询 ==========
    QList<LearningPath> getUserPaths(int user_id);
    LearningPath getPathDetail(int path_id, int user_id);

    // ========== 路径操作 ==========
    bool updatePathProgress(int path_id, int user_id, double progress);
    bool updatePathStatus(int path_id, int user_id, int status);
    bool deletePath(int path_id, int user_id);

    // ========== 阶段进度管理 ==========
    bool setStageCompleted(int path_id, int stage_order, bool completed);
    QList<int> getCompletedStages(int path_id);

    // ========== 步骤进度管理 ==========
    // 更新步骤完成状态并重新计算路径进度
    bool updateStepProgress(int path_id, int stage_order, int step_order, bool completed);
    // 计算路径的步骤进度（返回已完成步骤数/总步骤数）
    QPair<int, int> calculateStepProgress(int path_id);  // (已完成, 总数)
    // 当阶段完成状态变化时，同步更新path_data中所有步骤的状态
    bool syncStepsToStageCompletion(int path_id, int stage_order, bool stageCompleted);
    // 检查阶段的所有步骤是否都已完成
    bool areAllStepsCompleted(const QJsonArray &stepsArray);

    // ========== AI集成 ==========
    QString buildPathPrompt(const QString &learning_goal,
                            const QStringList &current_knowledge,
                            const QString &time_preference,
                            const QString &difficulty);
    QJsonObject parseAIResponse(const QString &ai_response);

private:
    LearningPathManager();
    LearningPathManager(const LearningPathManager&) = delete;
    LearningPathManager& operator=(const LearningPathManager&) = delete;

    QString _apiKey;

    // API配置
    QString getApiEndpoint() const { return "https://api.deepseek.com/chat/completions"; }
    QString getModel() const { return "deepseek-chat"; }

    // 数据库操作辅助方法
    bool savePathToDatabase(int user_id, const QString &path_name,
                           const QString &learning_goal, const QString &path_data);
};

#endif // LEARNINGPATHMANAGER_H
