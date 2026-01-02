#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QSqlDatabase>


class DatabaseManager
{
public:
    static DatabaseManager& getInstance();
    QSqlDatabase& getDatabase();

private:
    DatabaseManager();
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    QSqlDatabase _db;
    bool _isCreate;

};

#endif // DATABASEMANAGER_H
