#include "databasemanager.h"

#include <QCryptographicHash>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QCoreApplication>

DatabaseManager &DatabaseManager::getInstance()
{
    static DatabaseManager manager;
    if (!manager._isCreate) {
        // 使用 SQLite 替代 MySQL
        QString dbPath = QCoreApplication::applicationDirPath() + "/smartlearn.db";
        manager._db = QSqlDatabase::addDatabase("QSQLITE");
        manager._db.setDatabaseName(dbPath);

        if (!manager._db.open()) {
            qDebug() << "数据库连接失败: " << manager._db.lastError().text();
        } else {
            qDebug() << "数据库连接成功: " << dbPath;
            // 初始化数据库表
            manager.initDatabase();
        }
        manager._isCreate = true;
    }

    return manager;
}

QSqlDatabase &DatabaseManager::getDatabase()
{
    return _db;
}

DatabaseManager::DatabaseManager():_isCreate(false) {}

// ========== 数据库初始化 ==========

bool DatabaseManager::initDatabase()
{
    if (!_db.isOpen()) {
        qDebug() << "数据库未打开，无法初始化";
        return false;
    }

    return createTables();
}

bool DatabaseManager::createTables()
{
    QSqlQuery query(_db);

    // 创建用户表 (SQLite 语法)
    QString createUserTable = R"(
        CREATE TABLE IF NOT EXISTS users (
            user_id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT UNIQUE NOT NULL,
            password_hash TEXT NOT NULL,
            email TEXT UNIQUE,
            phone TEXT,
            grade TEXT,
            major TEXT,
            learning_goal TEXT,
            role TEXT DEFAULT 'student',
            status INTEGER DEFAULT 1,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            last_login TIMESTAMP
        )
    )";

    if (!query.exec(createUserTable)) {
        qDebug() << "创建用户表失败: " << query.lastError().text();
        return false;
    }

    // 检查并添加 learning_goal 字段（用于已存在的数据库）
    QString alterTable = R"(
        ALTER TABLE users ADD COLUMN learning_goal TEXT
    )";
    query.exec(alterTable);  // 如果字段已存在会失败，忽略错误

    // 创建用户知识掌握表
    QString createKnowledgeTable = R"(
        CREATE TABLE IF NOT EXISTS user_knowledge (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER NOT NULL,
            knowledge_point TEXT NOT NULL,
            mastery_level REAL DEFAULT 0.00,
            learned_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (user_id) REFERENCES users(user_id) ON DELETE CASCADE
        )
    )";

    if (!query.exec(createKnowledgeTable)) {
        qDebug() << "创建知识掌握表失败: " << query.lastError().text();
        return false;
    }

    // 创建操作日志表
    QString createLogTable = R"(
        CREATE TABLE IF NOT EXISTS user_logs (
            log_id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER,
            action_type TEXT NOT NULL,
            ip_address TEXT,
            action_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            details TEXT,
            FOREIGN KEY (user_id) REFERENCES users(user_id) ON DELETE SET NULL
        )
    )";

    if (!query.exec(createLogTable)) {
        qDebug() << "创建日志表失败: " << query.lastError().text();
        return false;
    }

    // 设置忙碌超时时间（毫秒）- 在 WAL 之前设置
    if (!query.exec("PRAGMA busy_timeout=5000")) {
        qDebug() << "设置 busy_timeout 失败: " << query.lastError().text();
    }

    // 启用 WAL 模式以提高并发性能
    if (!query.exec("PRAGMA journal_mode=WAL")) {
        qDebug() << "启用 WAL 模式失败: " << query.lastError().text();
        qDebug() << "注意：可能有其他程序正在访问数据库，请关闭其他实例后重试";
    } else {
        QString mode = query.next() ? query.value(0).toString() : "";
        qDebug() << "WAL 模式已启用:" << mode;
    }

    qDebug() << "数据库表创建成功";
    return true;
}

// ========== 用户操作 ==========

bool DatabaseManager::insertUser(const User &user)
{
    QSqlQuery query(_db);
    query.prepare("INSERT INTO users (username, password_hash, email, phone, grade, major, role, status) "
                  "VALUES (:username, :password_hash, :email, :phone, :grade, :major, :role, :status)");

    query.bindValue(":username", user.username);
    query.bindValue(":password_hash", user.password_hash);
    query.bindValue(":email", user.email.isEmpty() ? QVariant() : user.email);
    query.bindValue(":phone", user.phone.isEmpty() ? QVariant() : user.phone);
    query.bindValue(":grade", user.grade.isEmpty() ? QVariant() : user.grade);
    query.bindValue(":major", user.major.isEmpty() ? QVariant() : user.major);
    query.bindValue(":role", user.role == ROLE_ADMIN ? "admin" : "student");
    query.bindValue(":status", user.status);

    if (!query.exec()) {
        qDebug() << "插入用户失败: " << query.lastError().text();
        return false;
    }

    // 获取插入的行ID
    int lastId = query.lastInsertId().toInt();
    qDebug() << "用户已插入数据库 - 用户名:" << user.username << "ID:" << lastId;

    return true;
}

bool DatabaseManager::userExists(const QString &username)
{
    QSqlQuery query(_db);
    query.prepare("SELECT COUNT(*) FROM users WHERE username = :username");
    query.bindValue(":username", username);

    if (query.exec() && query.next()) {
        return query.value(0).toInt() > 0;
    }

    return false;
}

bool DatabaseManager::emailExists(const QString &email)
{
    if (email.isEmpty()) return false;

    QSqlQuery query(_db);
    query.prepare("SELECT COUNT(*) FROM users WHERE email = :email");
    query.bindValue(":email", email);

    if (query.exec() && query.next()) {
        return query.value(0).toInt() > 0;
    }

    return false;
}

bool DatabaseManager::checkUserLogin(const QString &username, const QString &password)
{
    QSqlQuery query(_db);
    query.prepare("SELECT password_hash FROM users WHERE username = :username AND status = 1");
    query.bindValue(":username", username);

    if (query.exec() && query.next()) {
        QString storedHash = query.value(0).toString();
        return verifyPassword(password, storedHash);
    }

    return false;
}

User DatabaseManager::getUserByUsername(const QString &username)
{
    User user;
    QSqlQuery query(_db);
    query.prepare("SELECT user_id, username, email, phone, grade, major, learning_goal, role, status, created_at, last_login "
                  "FROM users WHERE username = :username");
    query.bindValue(":username", username);

    if (query.exec() && query.next()) {
        user.user_id = query.value(0).toInt();
        user.username = query.value(1).toString();
        user.email = query.value(2).toString();
        user.phone = query.value(3).toString();
        user.grade = query.value(4).toString();
        user.major = query.value(5).toString();
        user.learning_goal = query.value(6).toString();
        QString roleStr = query.value(7).toString();
        user.role = (roleStr == "admin") ? ROLE_ADMIN : ROLE_STUDENT;
        user.status = query.value(8).toInt();
        user.created_at = query.value(9).toDateTime();
        user.last_login = query.value(10).toDateTime();
    }

    return user;
}

bool DatabaseManager::updateUserLastLogin(int user_id)
{
    QSqlQuery query(_db);
    query.prepare("UPDATE users SET last_login = CURRENT_TIMESTAMP WHERE user_id = :user_id");
    query.bindValue(":user_id", user_id);

    return query.exec();
}

// ========== 密码加密 ==========

QString DatabaseManager::hashPassword(const QString &password)
{
    // 使用 SHA-256 + 盐值
    QByteArray salt = QByteArrayLiteral("SmartLearn_Salt_2025");
    QByteArray data = password.toUtf8() + salt;
    QByteArray hash = QCryptographicHash::hash(data, QCryptographicHash::Sha256);
    return hash.toHex();
}

bool DatabaseManager::verifyPassword(const QString &password, const QString &hash)
{
    QString computedHash = hashPassword(password);
    return computedHash == hash;
}

// ========== 日志操作 ==========

bool DatabaseManager::logUserAction(int user_id, const QString &action_type,
                                   const QString &ip, const QString &details)
{
    QSqlQuery query(_db);
    query.prepare("INSERT INTO user_logs (user_id, action_type, ip_address, details) "
                  "VALUES (:user_id, :action_type, :ip_address, :details)");

    query.bindValue(":user_id", user_id > 0 ? user_id : QVariant());
    query.bindValue(":action_type", action_type);
    query.bindValue(":ip_address", ip);
    query.bindValue(":details", details);

    if (!query.exec()) {
        qDebug() << "记录日志失败: " << query.lastError().text();
        return false;
    }

    return true;
}

// ========== 学习目标操作 ==========

bool DatabaseManager::updateUserLearningGoal(int user_id, const QString &goal)
{
    QSqlQuery query(_db);
    query.prepare("UPDATE users SET learning_goal = :goal WHERE user_id = :user_id");
    query.bindValue(":goal", goal.isEmpty() ? QVariant() : goal);
    query.bindValue(":user_id", user_id);

    if (!query.exec()) {
        qDebug() << "更新学习目标失败: " << query.lastError().text();
        return false;
    }

    qDebug() << "学习目标已更新 - 用户ID:" << user_id << "目标:" << goal;
    return true;
}

// ========== 知识点操作 ==========

bool DatabaseManager::addKnowledgePoint(int user_id, const QString &knowledge_point, double mastery_level)
{
    // 检查知识点是否已存在
    QSqlQuery checkQuery(_db);
    checkQuery.prepare("SELECT COUNT(*) FROM user_knowledge WHERE user_id = :user_id AND knowledge_point = :point");
    checkQuery.bindValue(":user_id", user_id);
    checkQuery.bindValue(":point", knowledge_point);

    if (checkQuery.exec() && checkQuery.next() && checkQuery.value(0).toInt() > 0) {
        // 已存在，更新掌握程度
        QSqlQuery updateQuery(_db);
        updateQuery.prepare("UPDATE user_knowledge SET mastery_level = :level WHERE user_id = :user_id AND knowledge_point = :point");
        updateQuery.bindValue(":level", mastery_level);
        updateQuery.bindValue(":user_id", user_id);
        updateQuery.bindValue(":point", knowledge_point);
        return updateQuery.exec();
    }

    // 不存在，插入新记录
    QSqlQuery query(_db);
    query.prepare("INSERT INTO user_knowledge (user_id, knowledge_point, mastery_level) "
                  "VALUES (:user_id, :point, :level)");
    query.bindValue(":user_id", user_id);
    query.bindValue(":point", knowledge_point);
    query.bindValue(":level", mastery_level);

    if (!query.exec()) {
        qDebug() << "添加知识点失败: " << query.lastError().text();
        return false;
    }

    qDebug() << "知识点已添加 - 用户ID:" << user_id << "知识点:" << knowledge_point;
    return true;
}

bool DatabaseManager::removeKnowledgePoint(int user_id, const QString &knowledge_point)
{
    QSqlQuery query(_db);
    query.prepare("DELETE FROM user_knowledge WHERE user_id = :user_id AND knowledge_point = :point");
    query.bindValue(":user_id", user_id);
    query.bindValue(":point", knowledge_point);

    if (!query.exec()) {
        qDebug() << "删除知识点失败: " << query.lastError().text();
        return false;
    }

    qDebug() << "知识点已删除 - 用户ID:" << user_id << "知识点:" << knowledge_point;
    return true;
}

QStringList DatabaseManager::getUserKnowledgePoints(int user_id)
{
    QStringList points;
    QSqlQuery query(_db);
    query.prepare("SELECT knowledge_point FROM user_knowledge WHERE user_id = :user_id ORDER BY learned_at DESC");
    query.bindValue(":user_id", user_id);

    if (query.exec()) {
        while (query.next()) {
            points.append(query.value(0).toString());
        }
    } else {
        qDebug() << "获取知识点列表失败: " << query.lastError().text();
    }

    return points;
}

bool DatabaseManager::clearUserKnowledge(int user_id)
{
    QSqlQuery query(_db);
    query.prepare("DELETE FROM user_knowledge WHERE user_id = :user_id");
    query.bindValue(":user_id", user_id);

    if (!query.exec()) {
        qDebug() << "清空知识点失败: " << query.lastError().text();
        return false;
    }

    qDebug() << "用户知识点已清空 - 用户ID:" << user_id;
    return true;
}
