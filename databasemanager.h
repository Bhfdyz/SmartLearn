#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QSqlDatabase>
#include <QStringList>
#include "user.h"

class DatabaseManager
{
public:
    static DatabaseManager& getInstance();
    QSqlDatabase& getDatabase();

    // ========== 数据库初始化 ==========
    bool initDatabase();                           // 初始化数据库（创建表）
    bool createTables();                           // 创建所有表

    // ========== 用户操作 ==========
    bool insertUser(const User &user);             // 插入新用户
    bool userExists(const QString &username);      // 检查用户名是否存在
    bool emailExists(const QString &email);        // 检查邮箱是否存在
    bool checkUserLogin(const QString &username, const QString &password);  // 登录验证
    User getUserByUsername(const QString &username); // 根据用户名获取用户信息
    bool updateUserLastLogin(int user_id);         // 更新最后登录时间
    bool updateUserLearningGoal(int user_id, const QString &goal);  // 更新学习目标

    // ========== 知识点操作 ==========
    bool addKnowledgePoint(int user_id, const QString &knowledge_point, double mastery_level = 0.5);  // 添加知识点
    bool removeKnowledgePoint(int user_id, const QString &knowledge_point);  // 删除知识点
    QStringList getUserKnowledgePoints(int user_id);  // 获取用户的知识点列表
    bool clearUserKnowledge(int user_id);          // 清空用户的知识点

    // ========== 密码加密 ==========
    QString hashPassword(const QString &password); // 密码哈希加密
    bool verifyPassword(const QString &password, const QString &hash); // 验证密码

    // ========== 日志操作 ==========
    bool logUserAction(int user_id, const QString &action_type,
                      const QString &ip, const QString &details = ""); // 记录用户操作日志

private:
    DatabaseManager();
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    QSqlDatabase _db;
    bool _isCreate;

};

#endif // DATABASEMANAGER_H
