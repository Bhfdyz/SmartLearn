#ifndef CONFIG_H
#define CONFIG_H

// 连接设置
#define PORT 8080
#define HOSTPORT "127.0.0.1"


// 传输消息类型
#define LoginType "LoginType"
#define RegisterType "RegisterType"
#define SaveKnowledgeType "SaveKnowledgeType"      // 保存知识库
#define GetKnowledgeType "GetKnowledgeType"        // 获取知识库
#define AIChatType "AIChatType"                    // AI对话请求
#define GetAIChatHistoryType "GetAIChatHistoryType" // 获取AI对话历史
#define GetSessionListType "GetSessionListType"     // 获取会话列表
#define DeleteSessionType "DeleteSessionType"       // 删除会话

// 学习路径相关协议类型
#define GenerateLearningPathType "GenerateLearningPathType"    // 生成学习路径
#define GetLearningPathListType "GetLearningPathListType"      // 获取路径列表
#define GetLearningPathDetailType "GetLearningPathDetailType"  // 获取路径详情
#define UpdatePathProgressType "UpdatePathProgressType"        // 更新路径进度（阶段级别）
#define UpdateStepProgressType "UpdateStepProgressType"        // 更新步骤进度（步骤级别）
#define DeleteLearningPathType "DeleteLearningPathType"        // 删除路径

// 学习资源相关协议类型
#define GenerateResourcesType "GenerateResourcesType"          // AI生成资源推荐
#define GetResourcesType "GetResourcesType"                    // 获取资源列表
#define AddResourceType "AddResourceType"                      // 手动添加资源

// 注册错误码
enum RegisterErrorCode {
    REGISTER_SUCCESS = 0,        // 注册成功
    USERNAME_EXISTS = 1,         // 用户名已存在
    EMAIL_EXISTS = 2,            // 邮箱已注册
    INVALID_USERNAME = 3,        // 用户名格式错误
    INVALID_PASSWORD = 4,        // 密码格式错误
    INVALID_EMAIL = 5,           // 邮箱格式错误
    INVALID_PHONE = 6,           // 手机号格式错误
    DATABASE_ERROR = 7           // 数据库错误
};

#endif // CONFIG_H
