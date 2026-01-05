#include "registerdialog.h"
#include "ui_registerdialog.h"
#include "connectmanager.h"
#include "config.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QGroupBox>
#include <QAbstractSocket>

RegisterDialog::RegisterDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::RegisterDialog)
{
    ui->setupUi(this);

    // 设置窗口属性
    setWindowTitle("注册新账号");
    setFixedSize(550, 650);

    // 设置UI布局
    setupUI();

    // 加载样式
    setStyleSheet(R"(
        RegisterDialog {
            background-color: #f5f5f5;
        }
        QGroupBox {
            font-size: 16px;
            font-weight: bold;
            color: #333;
            border: 2px solid #2196F3;
            border-radius: 8px;
            margin-top: 10px;
            padding-top: 15px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 15px;
            padding: 0 5px;
        }
        QLineEdit {
            padding: 10px;
            border: 2px solid #ddd;
            border-radius: 6px;
            font-size: 14px;
            background-color: white;
        }
        QLineEdit:focus {
            border: 2px solid #2196F3;
        }
        QComboBox {
            padding: 10px;
            border: 2px solid #ddd;
            border-radius: 6px;
            font-size: 14px;
            background-color: white;
        }
        QComboBox::drop-down {
            border: none;
            width: 30px;
        }
        QComboBox::down-arrow {
            image: none;
            border-left: 5px solid transparent;
            border-right: 5px solid transparent;
            border-top: 5px solid #666;
            margin-right: 10px;
        }
        QPushButton {
            padding: 12px 40px;
            border: none;
            border-radius: 6px;
            font-size: 14px;
            font-weight: bold;
        }
        QPushButton#back_btn {
            background-color: #f44336;
            color: white;
        }
        QPushButton#back_btn:hover {
            background-color: #d32f2f;
        }
        QPushButton#back_btn:pressed {
            background-color: #b71c1c;
        }
        QPushButton#confirm_btn {
            background-color: #4CAF50;
            color: white;
        }
        QPushButton#confirm_btn:hover {
            background-color: #45a049;
        }
        QPushButton#confirm_btn:pressed {
            background-color: #388E3C;
        }
        QPushButton#confirm_btn:disabled {
            background-color: #cccccc;
            color: #666666;
        }
        QLabel {
            color: #333;
            font-size: 14px;
        }
        QLabel#title_label {
            font-size: 22px;
            font-weight: bold;
            color: #2196F3;
        }
        QLabel#required_label {
            color: #f44336;
            font-size: 12px;
        }
    )");

    // 获取Socket连接
    ConnectManager &manager = ConnectManager::getInstance();
    _client = manager.getSocket();

    // 连接信号槽
    connect(_username_edit, &QLineEdit::textChanged,
            this, &RegisterDialog::onUsernameChanged);
    connect(_password_edit, &QLineEdit::textChanged,
            this, &RegisterDialog::onPasswordChanged);
    connect(_confirm_password_edit, &QLineEdit::textChanged,
            this, &RegisterDialog::onConfirmPasswordChanged);
    connect(_back_btn, &QPushButton::clicked,
            this, &RegisterDialog::onBackToLogin);
    connect(_confirm_btn, &QPushButton::clicked,
            this, &RegisterDialog::onConfirmRegister);

    // 连接服务器响应信号槽（必须在构造函数中连接，而不是在点击按钮后）
    connect(_client, &QTcpSocket::readyRead,
            this, &RegisterDialog::SlotReadFromServer);
}

RegisterDialog::~RegisterDialog()
{
    delete ui;
}

void RegisterDialog::setupUI()
{
    // 主布局
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(30, 20, 30, 20);

    // 标题
    QLabel *titleLabel = new QLabel("注册新账号", this);
    titleLabel->setObjectName("title_label");
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    // 必填提示
    QLabel *requiredLabel = new QLabel("* 为必填项", this);
    requiredLabel->setObjectName("required_label");
    requiredLabel->setAlignment(Qt::AlignRight);
    mainLayout->addWidget(requiredLabel);

    // 注册表单组
    QGroupBox *formGroup = new QGroupBox("请填写注册信息", this);
    QGridLayout *formLayout = new QGridLayout(formGroup);
    formLayout->setSpacing(12);
    formLayout->setContentsMargins(20, 20, 20, 20);

    // 用户名
    QLabel *usernameLabel = new QLabel("* 用户名:", this);
    _username_edit = new QLineEdit(this);
    _username_edit->setPlaceholderText("4-20个字符，字母数字下划线");
    _username_tip = new QLabel("", this);
    _username_tip->setWordWrap(true);
    formLayout->addWidget(usernameLabel, 0, 0);
    formLayout->addWidget(_username_edit, 1, 0);
    formLayout->addWidget(_username_tip, 2, 0);

    // 密码
    QLabel *passwordLabel = new QLabel("* 密  码:", this);
    _password_edit = new QLineEdit(this);
    _password_edit->setEchoMode(QLineEdit::Password);
    _password_edit->setPlaceholderText("至少8位，包含字母和数字");
    _password_strength_label = new QLabel("", this);
    formLayout->addWidget(passwordLabel, 3, 0);
    formLayout->addWidget(_password_edit, 4, 0);
    formLayout->addWidget(_password_strength_label, 5, 0);

    // 确认密码
    QLabel *confirmLabel = new QLabel("* 确认密码:", this);
    _confirm_password_edit = new QLineEdit(this);
    _confirm_password_edit->setEchoMode(QLineEdit::Password);
    _confirm_password_edit->setPlaceholderText("请再次输入密码");
    formLayout->addWidget(confirmLabel, 6, 0);
    formLayout->addWidget(_confirm_password_edit, 7, 0);

    // 邮箱（可选）
    QLabel *emailLabel = new QLabel("  邮  箱:", this);
    _email_edit = new QLineEdit(this);
    _email_edit->setPlaceholderText("选填，如 example@email.com");
    formLayout->addWidget(emailLabel, 8, 0);
    formLayout->addWidget(_email_edit, 9, 0);

    // 手机号（可选）
    QLabel *phoneLabel = new QLabel("  手  机:", this);
    _phone_edit = new QLineEdit(this);
    _phone_edit->setPlaceholderText("选填，如 13800138000");
    formLayout->addWidget(phoneLabel, 10, 0);
    formLayout->addWidget(_phone_edit, 11, 0);

    // 年级（可选）
    QLabel *gradeLabel = new QLabel("  年  级:", this);
    _grade_combo = new QComboBox(this);
    _grade_combo->addItem("请选择");
    for (int year = 2025; year >= 2020; year--) {
        _grade_combo->addItem(QString::number(year));
    }
    formLayout->addWidget(gradeLabel, 12, 0);
    formLayout->addWidget(_grade_combo, 13, 0);

    // 专业（可选）
    QLabel *majorLabel = new QLabel("  专  业:", this);
    _major_edit = new QLineEdit(this);
    _major_edit->setPlaceholderText("选填，如 计算机科学与技术");
    formLayout->addWidget(majorLabel, 14, 0);
    formLayout->addWidget(_major_edit, 15, 0);

    mainLayout->addWidget(formGroup);

    // 按钮布局
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(20);
    btnLayout->addStretch();

    _back_btn = new QPushButton("返回登录", this);
    _back_btn->setObjectName("back_btn");
    _back_btn->setMinimumWidth(120);

    _confirm_btn = new QPushButton("确认注册", this);
    _confirm_btn->setObjectName("confirm_btn");
    _confirm_btn->setMinimumWidth(120);

    btnLayout->addWidget(_back_btn);
    btnLayout->addWidget(_confirm_btn);
    btnLayout->addStretch();

    mainLayout->addLayout(btnLayout);

    setLayout(mainLayout);
}

// ========== 验证方法 ==========

bool RegisterDialog::validateUsername(const QString &username)
{
    if (username.isEmpty()) {
        _username_tip->setText("");
        return false;
    }

    // 长度检查
    if (username.length() < 4 || username.length() > 20) {
        _username_tip->setText("⚠️ 用户名长度必须在4-20个字符之间");
        _username_tip->setStyleSheet("color: red; font-size: 12px;");
        return false;
    }

    // 格式检查：字母开头，只允许字母、数字、下划线
    QRegularExpression regex("^[a-zA-Z][a-zA-Z0-9_]*$");
    if (regex.match(username).hasMatch()) {
        _username_tip->setText("✅ 用户名格式正确");
        _username_tip->setStyleSheet("color: green; font-size: 12px;");
        return true;
    } else {
        _username_tip->setText("⚠️ 用户名只能包含字母、数字、下划线，且必须以字母开头");
        _username_tip->setStyleSheet("color: red; font-size: 12px;");
        return false;
    }
}

bool RegisterDialog::validatePassword(const QString &password)
{
    if (password.isEmpty()) {
        return false;
    }

    // 长度检查
    if (password.length() < 8) {
        return false;
    }

    // 复杂度检查：必须包含字母和数字
    bool hasLetter = false;
    bool hasDigit = false;

    for (const QChar &ch : password) {
        if (ch.isLetter()) hasLetter = true;
        if (ch.isDigit()) hasDigit = true;
    }

    return hasLetter && hasDigit;
}

bool RegisterDialog::validateEmail(const QString &email)
{
    if (email.isEmpty()) return true;  // 可选字段，空则通过

    QRegularExpression regex("^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$");
    return regex.match(email).hasMatch();
}

bool RegisterDialog::validatePhone(const QString &phone)
{
    if (phone.isEmpty()) return true;  // 可选字段，空则通过

    QRegularExpression regex("^1[3-9]\\d{9}$");
    return regex.match(phone).hasMatch();
}

bool RegisterDialog::validateInput()
{
    // 验证用户名
    if (!validateUsername(_username_edit->text())) {
        QMessageBox::warning(this, "输入错误", "用户名格式不正确");
        _username_edit->setFocus();
        return false;
    }

    // 验证密码
    if (!validatePassword(_password_edit->text())) {
        QMessageBox::warning(this, "输入错误", "密码强度不足（至少8位，包含字母和数字）");
        _password_edit->setFocus();
        return false;
    }

    // 验证两次密码一致
    if (_password_edit->text() != _confirm_password_edit->text()) {
        QMessageBox::warning(this, "输入错误", "两次输入的密码不一致");
        _confirm_password_edit->setFocus();
        return false;
    }

    // 验证邮箱
    if (!validateEmail(_email_edit->text())) {
        QMessageBox::warning(this, "输入错误", "邮箱格式不正确");
        _email_edit->setFocus();
        return false;
    }

    // 验证手机号
    if (!validatePhone(_phone_edit->text())) {
        QMessageBox::warning(this, "输入错误", "手机号格式不正确");
        _phone_edit->setFocus();
        return false;
    }

    return true;
}

// ========== UI更新方法 ==========

void RegisterDialog::updatePasswordStrength(const QString &password)
{
    if (password.isEmpty()) {
        _password_strength_label->setText("");
        return;
    }

    int strength = 0;

    // 长度评分
    if (password.length() >= 8) strength++;
    if (password.length() >= 12) strength++;

    // 复杂度评分
    bool hasLower = false, hasUpper = false, hasDigit = false, hasSpecial = false;
    for (const QChar &ch : password) {
        if (ch.isLower()) hasLower = true;
        else if (ch.isUpper()) hasUpper = true;
        else if (ch.isDigit()) hasDigit = true;
        else hasSpecial = true;
    }

    if (hasLower) strength++;
    if (hasUpper) strength++;
    if (hasDigit) strength++;
    if (hasSpecial) strength++;

    // 显示强度等级
    strength = qMin(strength, 5);

    QString text;
    QString color;
    QString bars;

    switch (strength) {
        case 0:
        case 1:
            text = "弱";
            color = "red";
            bars = "■□□□□";
            break;
        case 2:
            text = "较弱";
            color = "orange";
            bars = "■■□□□";
            break;
        case 3:
            text = "中等";
            color = "#FFC107";
            bars = "■■■□□";
            break;
        case 4:
            text = "较强";
            color = "#8BC34A";
            bars = "■■■■□";
            break;
        case 5:
            text = "强";
            color = "#4CAF50";
            bars = "■■■■■";
            break;
    }

    _password_strength_label->setText("强度： " + bars + " " + text);
    _password_strength_label->setStyleSheet("color: " + color + "; font-size: 12px;");
}

void RegisterDialog::clearErrorMessages()
{
    _username_tip->setText("");
    _password_strength_label->setText("");
}

void RegisterDialog::showError(const QString &message)
{
    QMessageBox::warning(this, "错误", message);
}

// ========== 槽函数 ==========

void RegisterDialog::onUsernameChanged(const QString &text)
{
    validateUsername(text);
}

void RegisterDialog::onPasswordChanged(const QString &text)
{
    updatePasswordStrength(text);
}

void RegisterDialog::onConfirmPasswordChanged(const QString &text)
{
    Q_UNUSED(text);
    // 可以在这里添加实时检查两次密码一致的逻辑
}

void RegisterDialog::onConfirmRegister()
{
    // 1. 检查socket连接状态
    if (_client->state() != QAbstractSocket::ConnectedState) {
        qDebug() << "Socket未连接，当前状态:" << _client->state();
        _client->abort();
        _client->connectToHost("127.0.0.1", 8080);
        if (!_client->waitForConnected(3000)) {
            QMessageBox::warning(this, "连接错误", "无法连接到服务器");
            _confirm_btn->setEnabled(true);
            _confirm_btn->setText("确认注册");
            return;
        }
        qDebug() << "Socket重新连接成功";
    }

    // 2. 本地验证
    if (!validateInput()) {
        return;
    }

    // 3. 禁用按钮，防止重复提交
    _confirm_btn->setEnabled(false);
    _confirm_btn->setText("注册中...");

    // 4. 构造JSON请求
    QJsonObject json;
    json["type"] = RegisterType;
    json["username"] = _username_edit->text();
    json["password"] = _password_edit->text();
    json["email"] = _email_edit->text().isEmpty() ? "" : _email_edit->text();
    json["phone"] = _phone_edit->text().isEmpty() ? "" : _phone_edit->text();
    json["grade"] = _grade_combo->currentText() == "请选择" ? "" : _grade_combo->currentText();
    json["major"] = _major_edit->text();
    json["role"] = "student";  // 默认为学生

    QJsonDocument doc(json);

    // 5. 发送请求
    qint64 bytesWritten = _client->write(doc.toJson());
    _client->flush();

    qDebug() << "发送注册请求:" << _username_edit->text() << "字节数:" << bytesWritten;
    qDebug() << "请求数据:" << doc.toJson();
}

void RegisterDialog::SlotReadFromServer()
{
    QByteArray data = _client->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject json = doc.object();

    QString type = json["type"].toString();
    if (type != "RegisterResponse") {
        return;  // 不是注册响应，忽略
    }

    QString status = json["status"].toString();
    QString message = json["message"].toString();

    if (status == "success") {
        QMessageBox::information(this, "注册成功", message);
        accept();  // 关闭对话框，返回登录界面
    } else {
        QMessageBox::warning(this, "注册失败", message);
        _confirm_btn->setEnabled(true);  // 重新启用按钮
        _confirm_btn->setText("确认注册");
    }
}

void RegisterDialog::onBackToLogin()
{
    reject();  // 关闭对话框，返回登录界面
}
