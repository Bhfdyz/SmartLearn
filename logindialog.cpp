#include "logindialog.h"
#include "ui_logindialog.h"
#include "connectmanager.h"

#include <QFile>
#include <QDebug>
#include <QMessageBox>
#include <QStyle>
#include <QJsonDocument>
#include <QJsonObject>

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::LoginDialog)
{
    ui->setupUi(this);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    this->setWindowTitle(tr("登录"));
    this->setFixedSize(1000, 600);
    QFile qss(":/res/qss/login.qss");
    if (qss.open(QFile::ReadOnly)) {
        QString style = QLatin1String(qss.readAll());
        this->setStyleSheet(style);
        qss.close();
    } else {
        qDebug() << "qss打开失败";
    }

    // 设置图片
    QPixmap pixmap(":/res/pic/login.png");
    ui->label->setPixmap(pixmap);
    ui->label->setScaledContents(true);
    ui->label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui->label->setMinimumWidth(100);
    // 设置lineedit
    QIcon user_icon(":/res/icon/user.svg");
    ui->user->addAction(user_icon, QLineEdit::LeadingPosition);
    ui->user->setPlaceholderText("请输入用户名");
    QIcon pass_icon(":/res/icon/password.svg");
    ui->password->addAction(pass_icon, QLineEdit::LeadingPosition);
    ui->password->setEchoMode(QLineEdit::Password);
    ui->password->setPlaceholderText("请输入密码");
    //设置socket
    ConnectManager &manager = ConnectManager::getInstance();
    _client = manager.getSocket();

}

LoginDialog::~LoginDialog()
{
    delete ui;
}

QString LoginDialog::getUser()
{
    return _user;
}

void LoginDialog::on_login_btn_clicked()
{
    _user = ui->user->text();
    _pass = ui->password->text();
    QJsonObject jsonobj;
    jsonobj.insert("user", ui->user->text());
    jsonobj.insert("password", ui->password->text());
    QJsonDocument jsondoc(jsonobj);
    _client->write(jsondoc.toJson());
    connect(_client, &QTcpSocket::readyRead,
            this, &LoginDialog::SlotReadFromServer);

    // QString command = "aaa";
    // _client->write(command.toUtf8());
    // connect(_client, &QTcpSocket::readyRead,
    //         this, &LoginDialog::SlotReadFromServer);
}

void LoginDialog::on_register_btn_clicked()
{

}

// 处理server回复
void LoginDialog::SlotReadFromServer()
{
    QString reply = _client->readAll();
    if (reply == "yes") {
        accept();
    } else if (reply == "no") {
        ui->message_label->setText(tr("    用户名或密码错误"));
    } else {
        qDebug() << "连接错误";
    }
}

