#include "logindialog.h"
#include "ui_logindialog.h"
#include "connectmanager.h"
#include "config.h"
#include "registerdialog.h"
#include "knowledgedialog.h"

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

    // 连接服务器响应信号槽
    connect(_client, &QTcpSocket::readyRead,
            this, &LoginDialog::SlotReadFromServer);
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
    jsonobj.insert("type", LoginType);
    jsonobj.insert("user", ui->user->text());
    jsonobj.insert("password", ui->password->text());
    QJsonDocument jsondoc(jsonobj);
    _client->write(jsondoc.toJson());
    _client->flush();
}

void LoginDialog::on_register_btn_clicked()
{
    // 创建注册对话框
    RegisterDialog registerDlg(this);

    // 以模态方式显示
    int result = registerDlg.exec();

    // 注册成功后返回登录界面
    if (result == QDialog::Accepted) {
        // 可选：清空登录输入框
        ui->user->clear();
        ui->password->clear();
        // 设置焦点到用户名输入框
        ui->user->setFocus();
    }
}

// 处理server回复
void LoginDialog::SlotReadFromServer()
{
    // 使用 peek() 查看数据但不消费，让RegisterDialog也能读取
    QByteArray data = _client->peek(_client->bytesAvailable());

    // 检查是否为JSON格式的响应
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
    if (!jsonDoc.isNull() && jsonDoc.isObject()) {
        QJsonObject jsonObj = jsonDoc.object();
        QString type = jsonObj["type"].toString();
        // 如果是注册响应或知识库响应，不处理
        if (type == "RegisterResponse" || type == "KnowledgeResponse") {
            return;
        }
    }

    // 处理登录响应 - 使用readAll()消费数据
    data = _client->readAll();
    QString reply = QString::fromUtf8(data);
    if (reply == "yes") {
        // 登录成功，打开知识库填写对话框
        KnowledgeDialog knowledgeDlg(_user, this);
        knowledgeDlg.exec();
        accept();
    } else if (reply == "no") {
        ui->message_label->setText(tr("    用户名或密码错误"));
    } else {
        qDebug() << "连接错误: " << reply;
    }
}

