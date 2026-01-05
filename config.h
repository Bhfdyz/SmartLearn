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
