#include "mainwindow.h"
#include "logindialog.h"

#include <QApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    LoginDialog login;
    MainWindow w;

    if (login.exec() == QDialog::Accepted) {
        w.show();
    } else {
        return 0; // 直接关闭login
    }

    return a.exec();
}
