#include "mainwindow.h"
#include "logindialog.h"

#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QDateTime>

// 日志处理函数
void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QString logMsg;
    switch (type) {
    case QtDebugMsg:
        logMsg = QString("[DEBUG] %1").arg(msg);
        break;
    case QtWarningMsg:
        logMsg = QString("[WARN] %1").arg(msg);
        break;
    case QtCriticalMsg:
        logMsg = QString("[CRITICAL] %1").arg(msg);
        break;
    case QtFatalMsg:
        logMsg = QString("[FATAL] %1").arg(msg);
        break;
    }

    QFile file("debug.log");
    file.open(QIODevice::Append | QIODevice::Text);
    QTextStream out(&file);
    out << QDateTime::currentDateTime().toString("hh:mm:ss") << " " << logMsg << "\n";
    file.close();
}

int main(int argc, char *argv[])
{
    // 安装消息处理程序
    qInstallMessageHandler(messageHandler);

    // 清空日志文件
    QFile::remove("debug.log");

    QApplication a(argc, argv);

    while (true) {
        LoginDialog login;
        if (login.exec() == QDialog::Accepted) {
            // 登录成功，显示主窗口
            QString username = login.getUser();
            MainWindow w(username);
            w.show();
            a.exec();

            // 主窗口关闭后，检查是否要重新登录
            // 如果是正常退出（不是点击退出登录按钮），则退出程序
            // 这里简单处理：主窗口关闭就退出整个程序
            break;
        } else {
            // 用户点击关闭按钮，退出程序
            break;
        }
    }

    return 0;
}
