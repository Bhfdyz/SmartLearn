#include "learningpathmanager.h"
#include "databasemanager.h"
#include "config.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QCoreApplication>
#include <QDateTime>

LearningPathManager &LearningPathManager::getInstance()
{
    static LearningPathManager manager;
    return manager;
}

LearningPathManager::LearningPathManager()
    : _apiKey("sk-b10a2912a78c4a47bb4908039e64f968")  // TODO: 移到配置文件
{
    qDebug() << "LearningPathManager 初始化完成";
}

// ========== 路径生成 ==========

QString LearningPathManager::buildPathPrompt(const QString &learning_goal,
                                              const QStringList &current_knowledge,
                                              const QString &time_preference,
                                              const QString &difficulty)
{
    QString knowledgeStr = current_knowledge.isEmpty() ? "无" : current_knowledge.join(", ");

    QString prompt = QString(R"(
你是一个专业的学习路径规划专家。请根据以下信息为学生生成一个详细的学习路径。

## 学生信息
- 学习目标：%1
- 已掌握知识点：%2
- 预期学习时长：%3
- 期望难度：%4

## 要求
1. 将学习路径划分为3-6个阶段
2. 每个阶段包含3-8个具体的学习步骤
3. 每个步骤应包含：标题、详细描述、关联知识点、推荐资源类型
4. 合理安排学习顺序，考虑知识点的前置关系
5. 估算每个阶段的学习时长

## 输出格式
请严格按照以下JSON格式输出，不要包含任何其他文字：

{
    "path_name": "学习路径名称",
    "total_estimated_hours": 总时长,
    "stages": [
        {
            "stage_order": 1,
            "stage_name": "阶段名称",
            "stage_description": "阶段描述",
            "estimated_hours": 时长,
            "steps": [
                {
                    "step_order": 1,
                    "step_title": "步骤标题",
                    "step_content": "步骤详细描述",
                    "knowledge_point": "关联的知识点",
                    "resource_type": "video/article/practice/project"
                }
            ]
        }
    ]
}
)").arg(learning_goal)
   .arg(knowledgeStr)
   .arg(time_preference)
   .arg(difficulty);

    return prompt;
}

QJsonObject LearningPathManager::parseAIResponse(const QString &ai_response)
{
    QJsonObject result;

    // 尝试直接解析JSON
    QJsonDocument doc = QJsonDocument::fromJson(ai_response.toUtf8());
    if (!doc.isNull() && doc.isObject()) {
        result = doc.object();
        return result;
    }

    // 如果直接解析失败，尝试提取JSON部分
    int jsonStart = ai_response.indexOf("{");
    int jsonEnd = ai_response.lastIndexOf("}");

    if (jsonStart >= 0 && jsonEnd > jsonStart) {
        QString jsonStr = ai_response.mid(jsonStart, jsonEnd - jsonStart + 1);
        doc = QJsonDocument::fromJson(jsonStr.toUtf8());
        if (!doc.isNull() && doc.isObject()) {
            result = doc.object();
            return result;
        }
    }

    qDebug() << "解析AI响应JSON失败:" << ai_response.left(200);
    return result;
}

GeneratePathResponse LearningPathManager::generatePath(int user_id,
                                                        const QString &path_name,
                                                        const QString &learning_goal,
                                                        const QStringList &current_knowledge,
                                                        const QString &time_preference,
                                                        const QString &difficulty)
{
    GeneratePathResponse response;
    response.success = false;

    qDebug() << "=== 开始生成学习路径 ===";
    qDebug() << "用户ID:" << user_id;
    qDebug() << "路径名称:" << path_name;
    qDebug() << "学习目标:" << learning_goal;
    qDebug() << "已掌握知识点:" << current_knowledge;

    // 构建Prompt
    QString prompt = buildPathPrompt(learning_goal, current_knowledge, time_preference, difficulty);

    // 构建请求消息
    QJsonArray messages;
    messages.append(QJsonObject{
        {"role", "system"},
        {"content", "你是专业的学习路径规划专家，请严格按照JSON格式输出学习路径。"}
    });
    messages.append(QJsonObject{
        {"role", "user"},
        {"content", prompt}
    });

    QJsonObject requestBody;
    requestBody["model"] = getModel();
    requestBody["messages"] = messages;
    requestBody["stream"] = false;
    requestBody["temperature"] = 0.7;
    requestBody["max_tokens"] = 4096;

    QJsonDocument doc(requestBody);
    QByteArray jsonData = doc.toJson();

    // 使用curl调用API
    QProcess process;
    QTemporaryFile tempFile;
    if (tempFile.open()) {
        tempFile.write(jsonData);
        tempFile.flush();
        tempFile.close();
    }

    QStringList args;
    args << "--connect-timeout" << "10";     // 连接超时 10 秒
    args << "--max-time" << "120";            // 最大总时间 120 秒（学习路径生成需要更长时间）
    args << "-X" << "POST";
    args << "-H" << "Content-Type: application/json";
    args << "-H" << "Accept: application/json";
    args << "-H" << ("Authorization: Bearer " + _apiKey);
    args << "-d" << ("@" + tempFile.fileName());
    args << getApiEndpoint();

    qDebug() << "curl命令: curl" << args.join(" ");

    process.start("curl", args);

    // 等待完成（150秒超时 = curl max-time 120秒 + 30秒缓冲）
    if (!process.waitForFinished(150000)) {
        response.error = "请求超时";
        qDebug() << "路径生成curl请求超时";
        return response;
    }

    QByteArray responseData = process.readAllStandardOutput();
    QByteArray errorData = process.readAllStandardError();

    qDebug() << "curl退出码:" << process.exitCode();
    if (!errorData.isEmpty()) {
        qDebug() << "curl错误输出:" << errorData;
    }
    qDebug() << "AI响应数据:" << responseData.left(500);

    if (process.exitCode() != 0) {
        response.error = "curl请求失败: " + QString::fromUtf8(errorData);
        return response;
    }

    // 解析响应
    QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);
    if (responseDoc.isNull()) {
        response.error = "解析JSON失败";
        qDebug() << response.error;
        return response;
    }

    QJsonObject responseObject = responseDoc.object();

    // 检查API错误
    if (responseObject.contains("error")) {
        QJsonObject errorObj = responseObject["error"].toObject();
        response.error = "API错误: " + errorObj["message"].toString();
        qDebug() << response.error;
        return response;
    }

    QJsonArray choices = responseObject["choices"].toArray();
    if (choices.isEmpty()) {
        response.error = "AI返回choices为空";
        return response;
    }

    QJsonObject choice = choices[0].toObject();
    QJsonObject messageObj = choice["message"].toObject();
    QString content = messageObj["content"].toString();

    // 解析AI返回的JSON
    QJsonObject pathData = parseAIResponse(content);
    if (pathData.isEmpty()) {
        response.error = "AI未返回有效的路径JSON";
        qDebug() << response.error << "\n原始内容:" << content;
        return response;
    }

    // 保存到数据库
    QJsonDocument pathDoc(pathData);
    QString pathDataStr = pathDoc.toJson(QJsonDocument::Compact);

    if (savePathToDatabase(user_id, path_name, learning_goal, pathDataStr)) {
        response.success = true;
        response.path_id = QSqlQuery(DatabaseManager::getInstance().getDatabase()).lastInsertId().toInt();
        response.path_data = pathData;
        qDebug() << "学习路径生成成功，path_id:" << response.path_id;
    } else {
        response.error = "保存路径到数据库失败";
    }

    return response;
}

bool LearningPathManager::savePathToDatabase(int user_id, const QString &path_name,
                                              const QString &learning_goal, const QString &path_data)
{
    QSqlDatabase db = DatabaseManager::getInstance().getDatabase();
    QSqlQuery query(db);

    query.prepare("INSERT INTO learning_paths (user_id, path_name, learning_goal, path_data, status, progress) "
                  "VALUES (:user_id, :path_name, :learning_goal, :path_data, 0, 0.0)");

    query.bindValue(":user_id", user_id);
    query.bindValue(":path_name", path_name);
    query.bindValue(":learning_goal", learning_goal);
    query.bindValue(":path_data", path_data);

    if (!query.exec()) {
        qDebug() << "保存学习路径失败: " << query.lastError().text();
        return false;
    }

    return true;
}

// ========== 路径查询 ==========

QList<LearningPath> LearningPathManager::getUserPaths(int user_id)
{
    QList<LearningPath> paths;
    QSqlDatabase db = DatabaseManager::getInstance().getDatabase();
    QSqlQuery query(db);

    query.prepare("SELECT path_id, user_id, path_name, learning_goal, path_data, status, progress, created_at, updated_at "
                  "FROM learning_paths WHERE user_id = :user_id ORDER BY created_at DESC");
    query.bindValue(":user_id", user_id);

    if (query.exec()) {
        while (query.next()) {
            LearningPath path;
            path.path_id = query.value(0).toInt();
            path.user_id = query.value(1).toInt();
            path.path_name = query.value(2).toString();
            path.learning_goal = query.value(3).toString();
            path.path_data = query.value(4).toString();
            path.status = query.value(5).toInt();
            path.progress = query.value(6).toDouble();
            path.created_at = query.value(7).toString();
            path.updated_at = query.value(8).toString();
            paths.append(path);
        }
    } else {
        qDebug() << "获取学习路径列表失败: " << query.lastError().text();
    }

    return paths;
}

LearningPath LearningPathManager::getPathDetail(int path_id, int user_id)
{
    LearningPath path;
    QSqlDatabase db = DatabaseManager::getInstance().getDatabase();
    QSqlQuery query(db);

    query.prepare("SELECT path_id, user_id, path_name, learning_goal, path_data, status, progress, created_at, updated_at "
                  "FROM learning_paths WHERE path_id = :path_id AND user_id = :user_id");
    query.bindValue(":path_id", path_id);
    query.bindValue(":user_id", user_id);

    if (query.exec() && query.next()) {
        path.path_id = query.value(0).toInt();
        path.user_id = query.value(1).toInt();
        path.path_name = query.value(2).toString();
        path.learning_goal = query.value(3).toString();
        path.path_data = query.value(4).toString();
        path.status = query.value(5).toInt();
        path.progress = query.value(6).toDouble();
        path.created_at = query.value(7).toString();
        path.updated_at = query.value(8).toString();
    } else {
        qDebug() << "获取学习路径详情失败: " << query.lastError().text();
    }

    return path;
}

// ========== 路径操作 ==========

bool LearningPathManager::updatePathProgress(int path_id, int user_id, double progress)
{
    QSqlDatabase db = DatabaseManager::getInstance().getDatabase();
    QSqlQuery query(db);

    query.prepare("UPDATE learning_paths SET progress = :progress, updated_at = CURRENT_TIMESTAMP "
                  "WHERE path_id = :path_id AND user_id = :user_id");
    query.bindValue(":progress", progress);
    query.bindValue(":path_id", path_id);
    query.bindValue(":user_id", user_id);

    if (!query.exec()) {
        qDebug() << "更新路径进度失败: " << query.lastError().text();
        return false;
    }

    return true;
}

bool LearningPathManager::updatePathStatus(int path_id, int user_id, int status)
{
    QSqlDatabase db = DatabaseManager::getInstance().getDatabase();
    QSqlQuery query(db);

    query.prepare("UPDATE learning_paths SET status = :status, updated_at = CURRENT_TIMESTAMP "
                  "WHERE path_id = :path_id AND user_id = :user_id");
    query.bindValue(":status", status);
    query.bindValue(":path_id", path_id);
    query.bindValue(":user_id", user_id);

    if (!query.exec()) {
        qDebug() << "更新路径状态失败: " << query.lastError().text();
        return false;
    }

    return true;
}

bool LearningPathManager::deletePath(int path_id, int user_id)
{
    QSqlDatabase db = DatabaseManager::getInstance().getDatabase();
    QSqlQuery query(db);

    query.prepare("DELETE FROM learning_paths WHERE path_id = :path_id AND user_id = :user_id");
    query.bindValue(":path_id", path_id);
    query.bindValue(":user_id", user_id);

    if (!query.exec()) {
        qDebug() << "删除学习路径失败: " << query.lastError().text();
        return false;
    }

    qDebug() << "学习路径已删除 - path_id:" << path_id << "user_id:" << user_id;
    return true;
}

// ========== 阶段进度管理 ==========

bool LearningPathManager::setStageCompleted(int path_id, int stage_order, bool completed)
{
    QSqlDatabase db = DatabaseManager::getInstance().getDatabase();
    QSqlQuery query(db);

    // 使用 INSERT OR REPLACE 来插入或更新记录
    query.prepare("INSERT OR REPLACE INTO path_stage_progress (path_id, stage_order, is_completed, completed_at) "
                  "VALUES (:path_id, :stage_order, :is_completed, CURRENT_TIMESTAMP)");
    query.bindValue(":path_id", path_id);
    query.bindValue(":stage_order", stage_order);
    query.bindValue(":is_completed", completed ? 1 : 0);

    if (!query.exec()) {
        qDebug() << "设置阶段完成状态失败: " << query.lastError().text();
        return false;
    }

    qDebug() << "阶段完成状态已更新 - path_id:" << path_id << "stage_order:" << stage_order << "completed:" << completed;
    return true;
}

QList<int> LearningPathManager::getCompletedStages(int path_id)
{
    QList<int> completedStages;
    QSqlDatabase db = DatabaseManager::getInstance().getDatabase();
    QSqlQuery query(db);

    query.prepare("SELECT stage_order FROM path_stage_progress WHERE path_id = :path_id AND is_completed = 1");
    query.bindValue(":path_id", path_id);

    if (query.exec()) {
        while (query.next()) {
            completedStages.append(query.value(0).toInt());
        }
    } else {
        qDebug() << "获取已完成阶段列表失败: " << query.lastError().text();
    }

    return completedStages;
}

// ========== 步骤进度管理 ==========

bool LearningPathManager::updateStepProgress(int path_id, int stage_order, int step_order, bool completed)
{
    QSqlDatabase db = DatabaseManager::getInstance().getDatabase();
    QSqlQuery query(db);

    // 1. 获取当前路径数据
    query.prepare("SELECT path_data, user_id FROM learning_paths WHERE path_id = :path_id");
    query.bindValue(":path_id", path_id);

    if (!query.exec()) {
        qDebug() << "获取路径数据失败: " << query.lastError().text();
        return false;
    }

    if (!query.next()) {
        qDebug() << "路径不存在: path_id=" << path_id;
        return false;
    }

    QString pathDataStr = query.value(0).toString();
    int user_id = query.value(1).toInt();

    // 2. 解析 path_data JSON
    QJsonDocument pathDoc = QJsonDocument::fromJson(pathDataStr.toUtf8());
    if (pathDoc.isNull() || !pathDoc.isObject()) {
        qDebug() << "path_data JSON 解析失败";
        return false;
    }

    QJsonObject pathObj = pathDoc.object();
    QJsonArray stagesArray = pathObj["stages"].toArray();

    // 3. 找到并更新对应步骤的 completed 状态
    bool stepFound = false;
    for (int i = 0; i < stagesArray.size(); ++i) {
        QJsonObject stageObj = stagesArray[i].toObject();
        if (stageObj["stage_order"].toInt() == stage_order) {
            QJsonArray stepsArray = stageObj["steps"].toArray();
            for (int j = 0; j < stepsArray.size(); ++j) {
                QJsonObject stepObj = stepsArray[j].toObject();
                if (stepObj["step_order"].toInt() == step_order) {
                    stepObj["completed"] = completed;
                    stepsArray[j] = stepObj;
                    stepFound = true;
                    break;
                }
            }
            stageObj["steps"] = stepsArray;
            stagesArray[i] = stageObj;
            break;
        }
    }

    if (!stepFound) {
        qDebug() << "未找到指定步骤: path_id=" << path_id << "stage_order=" << stage_order << "step_order=" << step_order;
        return false;
    }

    // 4. 保存更新后的 path_data
    pathObj["stages"] = stagesArray;
    QJsonDocument updatedDoc(pathObj);
    QString updatedPathData = QString::fromUtf8(updatedDoc.toJson(QJsonDocument::Compact));

    query.prepare("UPDATE learning_paths SET path_data = :path_data, updated_at = CURRENT_TIMESTAMP WHERE path_id = :path_id");
    query.bindValue(":path_data", updatedPathData);
    query.bindValue(":path_id", path_id);

    if (!query.exec()) {
        qDebug() << "更新 path_data 失败: " << query.lastError().text();
        return false;
    }

    // 4.5. 检查该阶段的所有步骤是否都已完成，同步更新阶段状态到 path_stage_progress 表
    for (int i = 0; i < stagesArray.size(); ++i) {
        QJsonObject stageObj = stagesArray[i].toObject();
        if (stageObj["stage_order"].toInt() == stage_order) {
            QJsonArray stepsArray = stageObj["steps"].toArray();

            bool allCompleted = areAllStepsCompleted(stepsArray);

            // 同步更新 path_stage_progress 表
            setStageCompleted(path_id, stage_order, allCompleted);

            qDebug() << "阶段状态已同步 - path_id:" << path_id << "stage_order:" << stage_order << "all_steps_completed:" << allCompleted;
            break;
        }
    }

    // 5. 重新计算整体进度（基于步骤）
    QPair<int, int> progress = calculateStepProgress(path_id);
    int completedSteps = progress.first;
    int totalSteps = progress.second;
    double newProgress = totalSteps > 0 ? (double)completedSteps / totalSteps : 0.0;

    updatePathProgress(path_id, user_id, newProgress);

    // 6. 根据进度更新状态
    int newStatus = 0;
    if (newProgress > 0) newStatus = 1;
    if (newProgress >= 1.0) newStatus = 2;
    updatePathStatus(path_id, user_id, newStatus);

    qDebug() << "步骤进度更新成功 - path_id:" << path_id << "stage_order:" << stage_order
             << "step_order:" << step_order << "completed:" << completed
             << "进度:" << completedSteps << "/" << totalSteps << "=" << newProgress;

    return true;
}

QPair<int, int> LearningPathManager::calculateStepProgress(int path_id)
{
    QSqlDatabase db = DatabaseManager::getInstance().getDatabase();
    QSqlQuery query(db);

    query.prepare("SELECT path_data FROM learning_paths WHERE path_id = :path_id");
    query.bindValue(":path_id", path_id);

    if (!query.exec()) {
        qDebug() << "获取路径数据失败: " << query.lastError().text();
        return QPair<int, int>(0, 0);
    }

    if (!query.next()) {
        qDebug() << "路径不存在: path_id=" << path_id;
        return QPair<int, int>(0, 0);
    }

    QString pathDataStr = query.value(0).toString();
    QJsonDocument pathDoc = QJsonDocument::fromJson(pathDataStr.toUtf8());

    if (pathDoc.isNull() || !pathDoc.isObject()) {
        return QPair<int, int>(0, 0);
    }

    QJsonObject pathObj = pathDoc.object();
    QJsonArray stagesArray = pathObj["stages"].toArray();

    int totalSteps = 0;
    int completedSteps = 0;

    for (const QJsonValue &stageValue : stagesArray) {
        QJsonObject stageObj = stageValue.toObject();
        QJsonArray stepsArray = stageObj["steps"].toArray();
        for (const QJsonValue &stepValue : stepsArray) {
            QJsonObject stepObj = stepValue.toObject();
            totalSteps++;
            if (stepObj["completed"].toBool()) {
                completedSteps++;
            }
        }
    }

    return QPair<int, int>(completedSteps, totalSteps);
}

bool LearningPathManager::areAllStepsCompleted(const QJsonArray &stepsArray)
{
    if (stepsArray.isEmpty()) {
        return false;  // 没有步骤时视为未完成
    }

    for (const QJsonValue &stepValue : stepsArray) {
        QJsonObject stepObj = stepValue.toObject();
        if (!stepObj["completed"].toBool()) {
            return false;  // 只要有一个步骤未完成，就返回false
        }
    }

    return true;  // 所有步骤都已完成
}

bool LearningPathManager::syncStepsToStageCompletion(int path_id, int stage_order, bool stageCompleted)
{
    QSqlDatabase db = DatabaseManager::getInstance().getDatabase();
    QSqlQuery query(db);

    // 1. 获取当前路径数据
    query.prepare("SELECT path_data FROM learning_paths WHERE path_id = :path_id");
    query.bindValue(":path_id", path_id);

    if (!query.exec()) {
        qDebug() << "获取路径数据失败: " << query.lastError().text();
        return false;
    }

    if (!query.next()) {
        qDebug() << "路径不存在: path_id=" << path_id;
        return false;
    }

    QString pathDataStr = query.value(0).toString();

    // 2. 解析 path_data JSON
    QJsonDocument pathDoc = QJsonDocument::fromJson(pathDataStr.toUtf8());
    if (pathDoc.isNull() || !pathDoc.isObject()) {
        qDebug() << "path_data JSON 解析失败";
        return false;
    }

    QJsonObject pathObj = pathDoc.object();
    QJsonArray stagesArray = pathObj["stages"].toArray();

    // 3. 找到对应阶段并同步步骤状态
    bool stageFound = false;
    bool stepsModified = false;

    for (int i = 0; i < stagesArray.size(); ++i) {
        QJsonObject stageObj = stagesArray[i].toObject();
        if (stageObj["stage_order"].toInt() == stage_order) {
            stageFound = true;
            QJsonArray stepsArray = stageObj["steps"].toArray();

            // 只有当阶段标记为完成时，才同步所有步骤为完成
            // 阶段标记为未完成时，保留步骤的原有状态
            if (stageCompleted) {
                for (int j = 0; j < stepsArray.size(); ++j) {
                    QJsonObject stepObj = stepsArray[j].toObject();
                    if (!stepObj["completed"].toBool()) {
                        stepObj["completed"] = true;
                        stepsArray[j] = stepObj;
                        stepsModified = true;
                    }
                }
            }

            stageObj["steps"] = stepsArray;
            stagesArray[i] = stageObj;
            break;
        }
    }

    if (!stageFound) {
        qDebug() << "未找到指定阶段: path_id=" << path_id << "stage_order=" << stage_order;
        return false;
    }

    // 4. 只有当步骤状态有变化时才保存
    if (stepsModified) {
        pathObj["stages"] = stagesArray;
        QJsonDocument updatedDoc(pathObj);
        QString updatedPathData = QString::fromUtf8(updatedDoc.toJson(QJsonDocument::Compact));

        query.prepare("UPDATE learning_paths SET path_data = :path_data, updated_at = CURRENT_TIMESTAMP WHERE path_id = :path_id");
        query.bindValue(":path_data", updatedPathData);
        query.bindValue(":path_id", path_id);

        if (!query.exec()) {
            qDebug() << "更新 path_data 失败: " << query.lastError().text();
            return false;
        }

        qDebug() << "步骤状态已同步到阶段完成状态 - path_id:" << path_id << "stage_order:" << stage_order << "stage_completed:" << stageCompleted;
    }

    return true;
}
