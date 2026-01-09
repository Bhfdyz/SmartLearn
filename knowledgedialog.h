#ifndef KNOWLEDGEDIALOG_H
#define KNOWLEDGEDIALOG_H

#include <QDialog>
#include <QTcpSocket>
#include <QLineEdit>
#include <QTextEdit>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>

QT_BEGIN_NAMESPACE
namespace Ui { class KnowledgeDialog; }
QT_END_NAMESPACE

class KnowledgeDialog : public QDialog
{
    Q_OBJECT

public:
    explicit KnowledgeDialog(const QString &username, QWidget *parent = nullptr);
    ~KnowledgeDialog();

public slots:
    void SlotReadFromServer();          // 接收服务器响应

private slots:
    void onAddKnowledge();              // 添加知识点
    void onRemoveKnowledge();           // 删除选中的知识点
    void onSave();                      // 保存知识库
    void onSkip();                      // 跳过

private:
    Ui::KnowledgeDialog *ui;
    QTcpSocket *_client;
    QString _username;

    // UI 组件
    QLineEdit *_goal_edit;              // 学习目标输入框
    QLineEdit *_knowledge_edit;         // 知识点输入框
    QPushButton *_add_btn;              // 添加按钮
    QPushButton *_remove_btn;           // 删除按钮
    QPushButton *_save_btn;             // 保存按钮
    QPushButton *_skip_btn;             // 跳过按钮
    QListWidget *_knowledge_list;       // 知识点列表
    QLabel *_count_label;               // 知识点计数标签

    void setupUI();                     // 设置UI布局
    void loadKnowledge();               // 从服务器加载已有知识点
};

#endif // KNOWLEDGEDIALOG_H
