#include "resourcemanager.h"
#include "databasemanager.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QProcess>
#include <QTemporaryFile>
#include <QDebug>

ResourceManager &ResourceManager::getInstance()
{
    static ResourceManager manager;
    return manager;
}

ResourceManager::ResourceManager()
    : _apiKey("sk-b10a2912a78c4a47bb4908039e64f968")  // TODO: 移到配置文件
{
    qDebug() << "ResourceManager 初始化完成";
}

ResourceManager::~ResourceManager()
{
}

ResourceGenerationResponse ResourceManager::generateAndSaveResources(
    int path_id,
    const QString &path_name,
    const QJsonArray &stages)
{
    ResourceGenerationResponse response;
    response.success = false;
    response.message = "资源生成开始";

    qDebug() << "=== 开始生成学习资源推荐 ===";
    qDebug() << "路径ID:" << path_id;
    qDebug() << "路径名称:" << path_name;
    qDebug() << "阶段数量:" << stages.size();

    // 清空该路径的旧资源
    DatabaseManager::getInstance().clearPathResources(path_id);

    QList<ResourceInfo> allResources;

    // 为每个阶段生成资源推荐
    for (int i = 0; i < stages.size(); ++i) {
        QJsonObject stage = stages[i].toObject();
        int stageOrder = stage["stage_order"].toInt();
        QString stageName = stage["stage_name"].toString();
        QString description = stage["description"].toString();

        qDebug() << "\n处理阶段" << stageOrder << ":" << stageName;

        // 调用AI生成资源推荐
        QString aiResponse = fetchAIResources(stageName, description);

        if (aiResponse.isEmpty()) {
            qDebug() << "阶段" << stageOrder << "AI生成失败，跳过";
            continue;
        }

        // 解析AI响应
        QList<ResourceInfo> stageResources = parseAIResponse(aiResponse, path_id, stageOrder);

        if (stageResources.isEmpty()) {
            qDebug() << "阶段" << stageOrder << "解析AI响应失败，跳过";
            continue;
        }

        qDebug() << "阶段" << stageOrder << "生成资源数量:" << stageResources.size();

        // 保存到数据库
        if (saveResourcesToDatabase(path_id, stageOrder, stageResources)) {
            allResources.append(stageResources);
        }
    }

    if (allResources.isEmpty()) {
        response.message = "未能生成任何资源推荐，请检查AI服务";
        response.success = false;
    } else {
        response.resources = allResources;
        response.message = QString("成功生成%1条学习资源推荐").arg(allResources.size());
        response.success = true;
    }

    qDebug() << "=== 资源生成完成，总数:" << allResources.size() << "===";
    return response;
}

QString ResourceManager::fetchAIResources(const QString &stage_name, const QString &description)
{
    // 构建AI提示词
    QString prompt = QString(
        "你是一个学习资源推荐助手。根据以下学习阶段信息，推荐5篇高质量的技术博客文章。\n\n"
        "学习阶段：%1\n"
        "阶段描述：%2\n\n"
        "请返回JSON格式数组，不要包含任何其他文字说明：\n"
        "[\n"
        "  {\n"
        "    \"title\": \"文章标题\",\n"
        "    \"url\": \"文章链接（必须是有效的URL，以http://或https://开头）\",\n"
        "    \"source\": \"来源平台（CSDN/掘金/知乎/博客园/简书等）\",\n"
        "    \"description\": \"简短描述（30字以内）\",\n"
        "    \"difficulty\": 1\n"
        "  }\n"
        "]\n\n"
        "要求：\n"
        "1. 优先推荐中文技术博客\n"
        "2. 来源包括：CSDN、掘金、知乎、博客园、简书等知名技术社区\n"
        "3. 确保链接格式正确（以http://或https://开头）\n"
        "4. 难度1-5级，1最简单，5最困难\n"
        "5. 只返回JSON数组，不要有其他说明文字"
    ).arg(stage_name).arg(description);

    // 构建请求体
    QJsonObject requestBody;
    requestBody["model"] = getModel();
    requestBody["messages"] = QJsonArray{
        QJsonObject{{"role", "system"}, {"content", "你是一个专业的学习资源推荐助手。"}},
        QJsonObject{{"role", "user"}, {"content", prompt}}
    };
    requestBody["stream"] = false;
    requestBody["temperature"] = 1.0;
    requestBody["max_tokens"] = 2048;

    QJsonDocument doc(requestBody);
    QByteArray jsonData = doc.toJson();

    qDebug() << "AI资源推荐请求:" << prompt.left(100) + "...";

    // 使用 curl 发送请求
    QProcess process;
    QTemporaryFile tempFile;

    if (!tempFile.open()) {
        qDebug() << "创建临时文件失败";
        return "";
    }

    tempFile.write(jsonData);
    tempFile.flush();
    tempFile.close();

    // 构建 curl 命令
    QStringList args;
    args << "--connect-timeout" << "10";
    args << "--max-time" << "90";  // 资源生成可能需要更长时间
    args << "-X" << "POST";
    args << "-H" << "Content-Type: application/json";
    args << "-H" << "Accept: application/json";
    args << "-H" << ("Authorization: Bearer " + _apiKey);
    args << "-d" << ("@" + tempFile.fileName());
    args << getApiEndpoint();

    process.start("curl", args);

    if (!process.waitForFinished(100000)) {  // 100秒超时
        qDebug() << "curl请求超时";
        return "";
    }

    QByteArray responseData = process.readAllStandardOutput();
    QByteArray errorData = process.readAllStandardError();

    if (process.exitCode() != 0) {
        qDebug() << "curl请求失败:" << errorData;
        return "";
    }

    qDebug() << "AI响应:" << responseData.left(200) + "...";

    // 解析响应
    QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);
    if (responseDoc.isNull() || !responseDoc.isObject()) {
        qDebug() << "解析JSON失败";
        return "";
    }

    QJsonObject responseObject = responseDoc.object();

    // 检查API错误
    if (responseObject.contains("error")) {
        qDebug() << "API错误:" << responseObject["error"].toObject()["message"].toString();
        return "";
    }

    QJsonArray choices = responseObject["choices"].toArray();
    if (choices.isEmpty()) {
        qDebug() << "AI返回choices为空";
        return "";
    }

    QJsonObject messageObj = choices[0].toObject()["message"].toObject();
    return messageObj["content"].toString();
}

QList<ResourceInfo> ResourceManager::parseAIResponse(const QString &ai_response, int path_id, int stage_order)
{
    QList<ResourceInfo> resources;

    // 尝试直接解析JSON数组
    QJsonDocument doc = QJsonDocument::fromJson(ai_response.toUtf8());

    if (!doc.isArray()) {
        // 如果不是数组，尝试查找JSON数组部分
        int arrayStart = ai_response.indexOf('[');
        int arrayEnd = ai_response.lastIndexOf(']');

        if (arrayStart >= 0 && arrayEnd > arrayStart) {
            QString jsonArray = ai_response.mid(arrayStart, arrayEnd - arrayStart + 1);
            doc = QJsonDocument::fromJson(jsonArray.toUtf8());
        }

        if (!doc.isArray()) {
            qDebug() << "AI响应不是有效的JSON数组";
            qDebug() << "原始响应:" << ai_response;
            return resources;
        }
    }

    QJsonArray array = doc.array();

    for (const QJsonValue &val : array) {
        if (!val.isObject()) continue;

        QJsonObject obj = val.toObject();
        ResourceInfo info;
        info.id = 0;
        info.path_id = path_id;
        info.stage_order = stage_order;
        info.title = obj["title"].toString();
        info.url = obj["url"].toString();
        info.source = obj["source"].toString();
        info.description = obj["description"].toString();
        info.difficulty = obj["difficulty"].toInt(1);
        info.created_at = "";

        // 验证必填字段
        if (!info.title.isEmpty() && !info.url.isEmpty()) {
            resources.append(info);
        }
    }

    return resources;
}

bool ResourceManager::saveResourcesToDatabase(int path_id, int stage_order, const QList<ResourceInfo> &resources)
{
    bool allSuccess = true;

    for (const ResourceInfo &info : resources) {
        bool success = DatabaseManager::getInstance().addResource(
            path_id,
            stage_order,
            info.title,
            info.url,
            info.source,
            info.description,
            info.difficulty
        );

        if (!success) {
            allSuccess = false;
        }
    }

    return allSuccess;
}

QList<ResourceInfo> ResourceManager::getPathResources(int path_id)
{
    return DatabaseManager::getInstance().getPathResources(path_id);
}

QList<ResourceInfo> ResourceManager::getStageResources(int path_id, int stage_order)
{
    return DatabaseManager::getInstance().getStageResources(path_id, stage_order);
}

bool ResourceManager::addResource(const ResourceInfo &resource)
{
    return DatabaseManager::getInstance().addResource(
        resource.path_id,
        resource.stage_order,
        resource.title,
        resource.url,
        resource.source,
        resource.description,
        resource.difficulty
    );
}
