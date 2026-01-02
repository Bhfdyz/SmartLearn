#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "databasemanager.h"

#include <QMainWindow>
#include <QTcpServer>
#include <QSqlDatabase>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    QTcpServer *_server;
    QSqlDatabase _db;

private slots:
    void SlotNewClient();
    void SlotReadFromClient();

};
#endif // MAINWINDOW_H
