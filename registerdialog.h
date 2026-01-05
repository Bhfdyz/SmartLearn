#ifndef REGISTERDIALOG_H
#define REGISTERDIALOG_H

#include <QDialog>
#include <QTcpSocket>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>

namespace Ui {
class RegisterDialog;
}

class RegisterDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RegisterDialog(QWidget *parent = nullptr);
    ~RegisterDialog();

private slots:
    void onConfirmRegister();        // 确认注册按钮点击
    void onBackToLogin();            // 返回登录按钮点击
    void onUsernameChanged(const QString &text);  // 用户名输入变化
    void onPasswordChanged(const QString &text);  // 密码输入变化
    void onConfirmPasswordChanged(const QString &text);  // 确认密码输入变化
    void SlotReadFromServer();       // 接收服务器响应

private:
    Ui::RegisterDialog *ui;
    QTcpSocket *_client;

    // UI 组件指针
    QLineEdit *_username_edit;
    QLineEdit *_password_edit;
    QLineEdit *_confirm_password_edit;
    QLineEdit *_email_edit;
    QLineEdit *_phone_edit;
    QComboBox *_grade_combo;
    QLineEdit *_major_edit;
    QLabel *_username_tip;
    QLabel *_password_strength_label;
    QPushButton *_confirm_btn;
    QPushButton *_back_btn;

    // ========== 验证方法 ==========
    bool validateInput();            // 验证所有输入
    bool validateUsername(const QString &username);  // 验证用户名
    bool validatePassword(const QString &password);  // 验证密码
    bool validateEmail(const QString &email);        // 验证邮箱
    bool validatePhone(const QString &phone);        // 验证手机号

    // ========== UI更新方法 ==========
    void updatePasswordStrength(const QString &password);  // 更新密码强度提示
    void clearErrorMessages();       // 清除错误提示
    void showError(const QString &message);  // 显示错误信息
    void setupUI();                  // 设置UI布局
};

#endif // REGISTERDIALOG_H
