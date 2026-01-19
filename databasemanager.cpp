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

    // 创建AI对话历史表
    QString createAIChatTable = R"(
        CREATE TABLE IF NOT EXISTS ai_chats (
            chat_id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER NOT NULL,
            role TEXT NOT NULL,
            content TEXT NOT NULL,
            session_id TEXT,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (user_id) REFERENCES users(user_id) ON DELETE CASCADE
        )
    )";

    if (!query.exec(createAIChatTable)) {
        qDebug() << "创建AI对话表失败: " << query.lastError().text();
        return false;
    }

    // 为session_id创建索引以提高查询性能
    query.exec("CREATE INDEX IF NOT EXISTS idx_ai_chats_session ON ai_chats(session_id)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_ai_chats_user ON ai_chats(user_id)");

    // 创建学习路径表
    QString createLearningPathTable = R"(
        CREATE TABLE IF NOT EXISTS learning_paths (
            path_id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER NOT NULL,
            path_name TEXT NOT NULL,
            learning_goal TEXT NOT NULL,
            path_data TEXT NOT NULL,
            status INTEGER DEFAULT 0,
            progress REAL DEFAULT 0.0,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (user_id) REFERENCES users(user_id) ON DELETE CASCADE
        )
    )";

    if (!query.exec(createLearningPathTable)) {
        qDebug() << "创建学习路径表失败: " << query.lastError().text();
        return false;
    }

    // 为学习路径表创建索引
    query.exec("CREATE INDEX IF NOT EXISTS idx_learning_paths_user ON learning_paths(user_id)");

    // 创建学习路径阶段进度表
    QString createStageProgressTable = R"(
        CREATE TABLE IF NOT EXISTS path_stage_progress (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            path_id INTEGER NOT NULL,
            stage_order INTEGER NOT NULL,
            is_completed BOOLEAN DEFAULT 0,
            completed_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (path_id) REFERENCES learning_paths(path_id) ON DELETE CASCADE,
            UNIQUE(path_id, stage_order)
        )
    )";

    if (!query.exec(createStageProgressTable)) {
        qDebug() << "创建阶段进度表失败: " << query.lastError().text();
        return false;
    }

    // 为阶段进度表创建索引
    query.exec("CREATE INDEX IF NOT EXISTS idx_stage_progress_path ON path_stage_progress(path_id)");

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

// ========== AI对话操作 ==========

QString DatabaseManager::generateSessionId()
{
    // 生成基于时间戳的会话ID
    return QString::number(QDateTime::currentMSecsSinceEpoch());
}

QString DatabaseManager::getLastSessionId(int user_id)
{
    QSqlQuery query(_db);
    query.prepare("SELECT session_id FROM ai_chats WHERE user_id = :user_id ORDER BY created_at DESC LIMIT 1");
    query.bindValue(":user_id", user_id);

    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }
    return "";  // 没有历史会话
}

bool DatabaseManager::saveAIChatMessage(int user_id, const QString &role, const QString &content, const QString &session_id)
{
    QSqlQuery query(_db);
    query.prepare("INSERT INTO ai_chats (user_id, role, content, session_id) "
                  "VALUES (:user_id, :role, :content, :session_id)");
    query.bindValue(":user_id", user_id);
    query.bindValue(":role", role);
    query.bindValue(":content", content);
    query.bindValue(":session_id", session_id);

    if (!query.exec()) {
        qDebug() << "保存AI对话失败: " << query.lastError().text();
        return false;
    }

    return true;
}

QList<QJsonObject> DatabaseManager::getAIChatHistory(int user_id, const QString &session_id, int limit)
{
    QList<QJsonObject> history;
    QSqlQuery query(_db);

    QString sql = "SELECT role, content, session_id, created_at FROM ai_chats "
                  "WHERE user_id = :user_id";
    if (!session_id.isEmpty()) {
        sql += " AND session_id = :session_id";
    }
    sql += " ORDER BY created_at ASC LIMIT :limit";

    query.prepare(sql);
    query.bindValue(":user_id", user_id);
    if (!session_id.isEmpty()) {
        query.bindValue(":session_id", session_id);
    }
    query.bindValue(":limit", limit);

    if (query.exec()) {
        while (query.next()) {
            QJsonObject msg;
            msg["role"] = query.value(0).toString();
            msg["content"] = query.value(1).toString();
            msg["session_id"] = query.value(2).toString();
            history.append(msg);
        }
    } else {
        qDebug() << "获取AI对话历史失败: " << query.lastError().text();
    }

    return history;
}

QList<QJsonObject> DatabaseManager::getSessionList(int user_id)
{
    QList<QJsonObject> sessions;
    QSqlQuery query(_db);

    // 获取每个会话的信息：session_id, 第一条消息（作为标题）, 创建时间, 消息数量
    QString sql = R"(
        SELECT
            session_id,
            MIN(created_at) as created_at,
            COUNT(*) as message_count
        FROM ai_chats
        WHERE user_id = :user_id
        GROUP BY session_id
        ORDER BY created_at DESC
    )";

    query.prepare(sql);
    query.bindValue(":user_id", user_id);

    if (query.exec()) {
        while (query.next()) {
            QString sessionId = query.value(0).toString();
            QString createdAt = query.value(1).toString();
            int messageCount = query.value(2).toInt();

            QJsonObject session;
            session["session_id"] = sessionId;
            session["created_at"] = createdAt;
            session["message_count"] = messageCount;

            // 获取会话标题（第一条用户消息）
            QString title = getSessionTitle(user_id, sessionId);
            session["title"] = title.isEmpty() ? "新对话" : title;

            sessions.append(session);
        }
    } else {
        qDebug() << "获取会话列表失败: " << query.lastError().text();
    }

    return sessions;
}

bool DatabaseManager::deleteSession(int user_id, const QString &session_id)
{
    QSqlQuery query(_db);
    query.prepare("DELETE FROM ai_chats WHERE user_id = :user_id AND session_id = :session_id");
    query.bindValue(":user_id", user_id);
    query.bindValue(":session_id", session_id);

    if (!query.exec()) {
        qDebug() << "删除会话失败: " << query.lastError().text();
        return false;
    }

    qDebug() << "会话已删除 - user_id:" << user_id << "session_id:" << session_id
             << "影响行数:" << query.numRowsAffected();
    return true;
}

QString DatabaseManager::getSessionTitle(int user_id, const QString &session_id)
{
    QSqlQuery query(_db);
    // 获取会话中第一条用户消息作为标题
    query.prepare(R"(
        SELECT content FROM ai_chats
        WHERE user_id = :user_id AND session_id = :session_id AND role = 'user'
        ORDER BY created_at ASC LIMIT 1
    )");
    query.bindValue(":user_id", user_id);
    query.bindValue(":session_id", session_id);

    if (query.exec() && query.next()) {
        QString content = query.value(0).toString();
        // 截取前30个字符作为标题
        if (content.length() > 30) {
            return content.left(30) + "...";
        }
        return content;
    }

    return "";  // 没有用户消息
}
