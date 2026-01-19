#include "resourceitem.h"

ResourceItem::ResourceItem(const ResourceInfo &info, QWidget *parent)
    : QWidget(parent)
    , _info(info)
    , _url(info.url)
{
    setupUI(info);
}

QSize ResourceItem::sizeHint() const
{
    return QSize(0, 140);
}

void ResourceItem::setupUI(const ResourceInfo &info)
{
    setStyleSheet("background-color: white; border-radius: 8px;");
    setFixedHeight(140);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(15, 12, 15, 12);
    mainLayout->setSpacing(8);

    // 顶部行：来源标签 + 难度星级
    QHBoxLayout *topLayout = new QHBoxLayout();
    topLayout->setSpacing(10);

    // 来源标签
    QString source = info.source.isEmpty() ? "未知来源" : info.source;
    QString sourceColor = getSourceBadgeColor(source);

    _sourceLabel = new QLabel(source);
    _sourceLabel->setStyleSheet(QString(
        "QLabel {"
        "   background-color: %1;"
        "   color: white;"
        "   padding: 3px 10px;"
        "   border-radius: 4px;"
        "   font-size: 11px;"
        "   font-weight: bold;"
        "}"
    ).arg(sourceColor));
    _sourceLabel->setMaximumWidth(80);
    topLayout->addWidget(_sourceLabel);

    topLayout->addStretch();

    // 难度显示
    QString difficultyText = getDifficultyStars(info.difficulty);
    _difficultyLabel = new QLabel(difficultyText);
    _difficultyLabel->setStyleSheet("color: #f39c12; font-size: 12px; font-weight: bold;");
    topLayout->addWidget(_difficultyLabel);

    mainLayout->addLayout(topLayout);

    // 标题
    _titleLabel = new QLabel(info.title);
    _titleLabel->setStyleSheet(
        "color: #2c3e50;"
        "font-size: 15px;"
        "font-weight: bold;"
    );
    _titleLabel->setWordWrap(true);
    mainLayout->addWidget(_titleLabel);

    // 描述
    _descriptionLabel = new QLabel(info.description.isEmpty() ? "暂无描述" : info.description);
    _descriptionLabel->setStyleSheet(
        "color: #7f8c8d;"
        "font-size: 12px;"
    );
    _descriptionLabel->setWordWrap(true);
    mainLayout->addWidget(_descriptionLabel);

    // 底部行：URL + 打开按钮
    QHBoxLayout *bottomLayout = new QHBoxLayout();
    bottomLayout->setSpacing(10);

    // URL显示（截断）
    QString displayUrl = info.url;
    if (displayUrl.length() > 50) {
        displayUrl = displayUrl.left(47) + "...";
    }
    _urlLabel = new QLabel("🔗 " + displayUrl);
    _urlLabel->setStyleSheet(
        "color: #3498db;"
        "font-size: 11px;"
    );
    _urlLabel->setWordWrap(true);
    bottomLayout->addWidget(_urlLabel, 1);

    // 打开按钮
    _openBtn = new QPushButton("打开");
    _openBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #3498db;
            color: white;
            border: none;
            padding: 6px 16px;
            border-radius: 5px;
            font-size: 12px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: #2980b9;
        }
        QPushButton:pressed {
            background-color: #21618c;
        }
    )");
    _openBtn->setFixedWidth(70);
    connect(_openBtn, &QPushButton::clicked, this, &ResourceItem::onOpenButtonClicked);
    bottomLayout->addWidget(_openBtn);

    mainLayout->addLayout(bottomLayout);
}

void ResourceItem::onOpenButtonClicked()
{
    emit itemClicked(_url);
}

QString ResourceItem::getSourceBadgeColor(const QString &source)
{
    QString s = source.toLower();
    if (s.contains("csdn")) return "#e74c3c";      // 红色
    if (s.contains("掘金") || s.contains("juejin")) return "#3498db";  // 蓝色
    if (s.contains("知乎")) return "#9b59b6";       // 紫色
    if (s.contains("博客园") || s.contains("cnblogs")) return "#27ae60";  // 绿色
    if (s.contains("简书")) return "#f39c12";       // 橙色
    if (s.contains("github")) return "#34495e";     // 深灰色
    if (s.contains("stack")) return "#f77f00";      // 橙黄色
    return "#95a5a6";  // 默认灰色
}

QString ResourceItem::getDifficultyStars(int difficulty)
{
    QString stars;
    int level = qBound(1, difficulty, 5);

    for (int i = 0; i < 5; ++i) {
        if (i < level) {
            stars += "★";
        } else {
            stars += "☆";
        }
    }

    return stars;
}
