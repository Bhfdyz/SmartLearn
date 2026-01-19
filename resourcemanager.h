#ifndef RESOURCEMANAGER_H
#define RESOURCEMANAGER_H

#include <QObject>
#include <QString>
#include <QList>
#include <QJsonObject>
#include <QJsonArray>
#include "databasemanager.h"  // ResourceInfo定义在此处

// 资源生成响应结构
struct ResourceGenerationResponse {
    bool success;
    QString message;
    QList<ResourceInfo> resources;
};

class ResourceManager
{
public:
    static ResourceManager& getInstance();

    // AI生成并保存资源推荐
    ResourceGenerationResponse generateAndSaveResources(
        int path_id,
        const QString &path_name,
        const QJsonArray &stages
    );

    // 获取路径的所有资源
    QList<ResourceInfo> getPathResources(int path_id);

    // 获取特定阶段的资源
    QList<ResourceInfo> getStageResources(int path_id, int stage_order);

    // 手动添加资源
    bool addResource(const ResourceInfo &resource);

private:
    ResourceManager();
    ~ResourceManager();
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;

    // 调用AI生成资源推荐（同步）
    QString fetchAIResources(const QString &stage_name, const QString &description);

    // 解析AI返回的JSON
    QList<ResourceInfo> parseAIResponse(const QString &ai_response, int path_id, int stage_order);

    // 保存资源到数据库
    bool saveResourcesToDatabase(int path_id, int stage_order, const QList<ResourceInfo> &resources);

    // API配置
    QString _apiKey;
    QString getApiEndpoint() const { return "https://api.deepseek.com/chat/completions"; }
    QString getModel() const { return "deepseek-chat"; }
};

#endif // RESOURCEMANAGER_H
