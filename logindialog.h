#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>
#include <QTcpServer>

namespace Ui {
class LoginDialog;
}

class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(QWidget *parent = nullptr);
    ~LoginDialog();
    QString getUser();

private:
    Ui::LoginDialog *ui;
    QString _user;
    QString _pass;
    QTcpSocket *_client;

signals:
    void SigLogin(const QString&);

private slots:
    void on_login_btn_clicked();

    void on_register_btn_clicked();

    void SlotReadFromServer();
};

#endif // LOGINDIALOG_H
