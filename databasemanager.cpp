#include "databasemanager.h"

DatabaseManager &DatabaseManager::getInstance()
{
    static DatabaseManager manager;
    if (!manager._isCreate) {
        manager._db = QSqlDatabase::addDatabase("QMYSQL");
        manager._db.setHostName("localhost");  // 数据库服务器地址
        manager._db.setDatabaseName("db1"); // 数据库名
        manager._db.setUserName("root"); // 用户名
        manager._db.setPassword("1234"); // 密码
    }

    return manager;
}

QSqlDatabase &DatabaseManager::getDatabase()
{
    return _db;
}

DatabaseManager::DatabaseManager():_isCreate(false) {}
