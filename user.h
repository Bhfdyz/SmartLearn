#ifndef USER_H
#define USER_H

#include <QString>
#include <QDateTime>
#include <QMetaType>

/**
 * @brief 用户角色枚举
 */
enum UserRole {
    ROLE_STUDENT = 0,    // 学生
    ROLE_ADMIN = 1       // 管理员
};

/**
 * @brief 用户信息结构体
 */
struct User {
    int user_id;              // 用户ID
    QString username;         // 用户名
    QString password_hash;    // 密码哈希值
    QString email;            // 邮箱
    QString phone;            // 手机号
    QString grade;            // 年级
    QString major;            // 专业
    UserRole role;            // 角色
    int status;               // 状态：1-正常，0-禁用
    QDateTime created_at;     // 创建时间
    QDateTime last_login;     // 最后登录时间

    // 默认构造函数
    User() : user_id(0), role(ROLE_STUDENT), status(1) {}
};

Q_DECLARE_METATYPE(User)

#endif // USER_H
