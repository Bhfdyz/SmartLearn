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
#include <QJsonArray>

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
    // 断开LoginDialog的信号连接，避免与RegisterDialog冲突
    disconnect(_client, &QTcpSocket::readyRead, this, &LoginDialog::SlotReadFromServer);

    // 创建注册对话框
    RegisterDialog registerDlg(this);

    // 以模态方式显示
    int result = registerDlg.exec();

    // 重新连接LoginDialog的信号
    connect(_client, &QTcpSocket::readyRead, this, &LoginDialog::SlotReadFromServer);

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
    QByteArray data = _client->readAll();
    qDebug() << "LoginDialog收到数据:" << data;

    // 检查是否为JSON格式的响应
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
    if (!jsonDoc.isNull() && jsonDoc.isObject()) {
        QJsonObject jsonObj = jsonDoc.object();
        QString type = jsonObj["type"].toString();
        qDebug() << "LoginDialog收到的JSON类型:" << type;
        // 如果是注册响应或知识库响应，不处理
        if (type == "RegisterResponse" || type == "KnowledgeResponse") {
            qDebug() << "LoginDialog忽略" << type << "消息";
            return;
        }
    }

    // 处理登录响应
    QString reply = QString::fromUtf8(data);
    if (reply == "yes") {
        qDebug() << "登录成功，检查用户知识库状态";

        // 断开LoginDialog的信号连接，避免冲突
        disconnect(_client, &QTcpSocket::readyRead, this, &LoginDialog::SlotReadFromServer);

        // 先查询用户是否已有知识库数据
        QJsonObject json;
        json["type"] = GetKnowledgeType;
        json["username"] = _user;
        QJsonDocument doc(json);
        _client->write(doc.toJson());
        _client->flush();

        // 等待知识库响应
        if (_client->waitForReadyRead(3000)) {
            QByteArray responseData = _client->readAll();
            QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);

            if (!responseDoc.isNull() && responseDoc.isObject()) {
                QJsonObject responseJson = responseDoc.object();
                QString type = responseJson["type"].toString();
                QString status = responseJson["status"].toString();

                if (type == "KnowledgeResponse" && status == "success") {
                    QJsonArray knowledgeArray = responseJson["knowledge_points"].toArray();

                    if (knowledgeArray.isEmpty()) {
                        // 用户没有知识库数据，弹出填写对话框
                        qDebug() << "用户无知识库数据，打开填写对话框";
                        KnowledgeDialog knowledgeDlg(_user, this);
                        knowledgeDlg.exec();
                    } else {
                        qDebug() << "用户已有知识库数据，直接进入主窗口";
                    }
                } else {
                    // 查询失败，默认弹出填写对话框
                    qDebug() << "查询知识库失败，打开填写对话框";
                    KnowledgeDialog knowledgeDlg(_user, this);
                    knowledgeDlg.exec();
                }
            } else {
                // 响应解析失败，默认弹出填写对话框
                qDebug() << "知识库响应解析失败，打开填写对话框";
                KnowledgeDialog knowledgeDlg(_user, this);
                knowledgeDlg.exec();
            }
        } else {
            // 查询超时，默认弹出填写对话框
            qDebug() << "查询知识库超时，打开填写对话框";
            KnowledgeDialog knowledgeDlg(_user, this);
            knowledgeDlg.exec();
        }

        // 登录成功，对话框即将关闭，不需要重新连接信号
        accept();
    } else if (reply == "no") {
        ui->message_label->setText(tr("    用户名或密码错误"));
    } else {
        qDebug() << "连接错误: " << reply;
    }
}

