#ifndef RESOURCEITEM_H
#define RESOURCEITEM_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include "resourcepage.h"

class ResourceItem : public QWidget
{
    Q_OBJECT

public:
    explicit ResourceItem(const ResourceInfo &info, QWidget *parent = nullptr);
    QSize sizeHint() const override;

signals:
    void itemClicked(const QString &url);  // 点击打开链接信号

private slots:
    void onOpenButtonClicked();

private:
    void setupUI(const ResourceInfo &info);
    QString getSourceBadgeColor(const QString &source);  // 获取来源标签颜色
    QString getDifficultyStars(int difficulty);          // 获取难度星级

    ResourceInfo _info;
    QString _url;
    QLabel *_titleLabel;
    QLabel *_sourceLabel;
    QLabel *_descriptionLabel;
    QLabel *_urlLabel;
    QLabel *_difficultyLabel;
    QPushButton *_openBtn;
};

#endif // RESOURCEITEM_H
