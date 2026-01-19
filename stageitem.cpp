#include "stageitem.h"
#include <QDebug>

StageItem::StageItem(const PathStage &stage, int index, QWidget *parent)
    : QWidget(parent)
    , _index(index)
    , _stage(stage)
    , _expanded(stage.expanded)
{
    setStyleSheet("background-color: transparent;");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 8, 5, 8);  // 减少边距，避免内容被裁剪
    mainLayout->setSpacing(4);  // 组件间距

    // 顶部行：复选框 + 标题
    QHBoxLayout *topLayout = new QHBoxLayout();
    topLayout->setContentsMargins(5, 0, 5, 0);  // 减少左右边距
    topLayout->setSpacing(8);  // 组件间距

    // 复选框
    _checkbox = new QCheckBox(this);
    _checkbox->setChecked(_stage.completed);
    _checkbox->setStyleSheet(R"(
        QCheckBox {
            font-size: 16px;
            spacing: 8px;
        }
        QCheckBox::indicator {
            width: 20px;
            height: 20px;
            border: 2px solid #bdc3c7;
            border-radius: 4px;
            background-color: white;
        }
        QCheckBox::indicator:hover {
            border: 2px solid #3498db;
        }
        QCheckBox::indicator:checked {
            background-color: #27ae60;
            border: 2px solid #27ae60;
        }
        QCheckBox::indicator:checked:hover {
            background-color: #229954;
            border: 2px solid #229954;
        }
    )");
    connect(_checkbox, &QCheckBox::stateChanged, this, &StageItem::onCheckboxStateChanged);
    topLayout->addWidget(_checkbox);

    // 展开/收起图标 + 标题
    _label = new QLabel(this);
    _label->setTextFormat(Qt::RichText);  // 修复：改为 RichText 以支持 HTML 链接
    _label->setOpenExternalLinks(false);  // 阻止打开外部链接
    _label->setStyleSheet(R"(
        QLabel {
            color: #2c3e50;
            font-size: 14px;
            padding: 4px 8px;
            border-radius: 4px;
        }
        QLabel:hover {
            background-color: #f8f9fa;
            color: #3498db;
        }
    )");
    _label->setCursor(Qt::PointingHandCursor);
    connect(_label, &QLabel::linkActivated, this, &StageItem::onLabelClicked);

    topLayout->addWidget(_label, 1);
    mainLayout->addLayout(topLayout);

    // 详情标签（显示预计时间和步骤统计）
    _detailsLabel = new QLabel(this);
    _detailsLabel->setTextFormat(Qt::RichText);  // 改为 RichText
    _detailsLabel->setWordWrap(true);
    _detailsLabel->setMinimumHeight(25);  // 确保收起时也有足够高度显示
    _detailsLabel->setStyleSheet(R"(
        QLabel {
            color: #7f8c8d;
            font-size: 13px;
            padding: 4px 8px;  // 移除左边距 44px，使用统一的小边距
        }
    )");
    mainLayout->addWidget(_detailsLabel);

    // 步骤容器（用于放置步骤复选框）
    _stepsContainer = new QWidget(this);
    _stepsLayout = new QVBoxLayout(_stepsContainer);
    _stepsLayout->setContentsMargins(10, 5, 10, 5);
    _stepsLayout->setSpacing(3);
    _stepsContainer->setLayout(_stepsLayout);
    mainLayout->addWidget(_stepsContainer);

    updateDisplay();
}

QSize StageItem::sizeHint() const
{
    // 复选框行高度（包括padding）
    int baseHeight = 50;
    // 详情行（预计时间+步骤数）- 收起时也显示
    int detailsHeight = 25;

    int totalHeight = baseHeight + detailsHeight;

    // 如果展开，加上步骤的高度
    if (_expanded && !_stage.steps.isEmpty()) {
        totalHeight += 10 + _stage.steps.size() * 30;  // 每个步骤约30px
    }

    return QSize(600, totalHeight);  // 设置最小宽度确保布局正常
}

void StageItem::setExpanded(bool expanded)
{
    if (_expanded != expanded) {
        _expanded = expanded;
        _stage.expanded = expanded;
        updateDisplay();
    }
}

void StageItem::setStepCompleted(int stepIndex, bool completed)
{
    if (stepIndex >= 0 && stepIndex < _stepCheckboxes.size()) {
        _stepCheckboxes[stepIndex]->setChecked(completed);
    }
}

void StageItem::setAllStepsCompleted(bool completed)
{
    // 更新本地数据
    for (int i = 0; i < _stage.steps.size(); ++i) {
        QJsonObject stepObj = _stage.steps[i];
        stepObj["completed"] = completed;
        _stage.steps[i] = stepObj;
    }

    // 更新现有复选框状态（不重新创建UI）
    for (QCheckBox *checkbox : _stepCheckboxes) {
        checkbox->setChecked(completed);
    }

    // 只更新详情标签的完成统计
    int completedSteps = completed ? _stage.steps.size() : 0;
    QString detailsText = "   预计: " + QString::number(_stage.estimated_hours) + "小时"
                         + " | " + QString::number(_stage.steps.size()) + "个步骤"
                         + " | 已完成: " + QString::number(completedSteps) + "/" + QString::number(_stage.steps.size());
    _detailsLabel->setText(detailsText);
}

void StageItem::updateStageData(const PathStage &stage)
{
    _stage = stage;
    _expanded = stage.expanded;
    _checkbox->setChecked(stage.completed);
    updateDisplay();
}

void StageItem::createStepCheckboxes()
{
    // 清除旧的复选框和布局 - 使用 takeAt() 立即删除布局项
    while (!_stepsLayout->isEmpty()) {
        QLayoutItem *item = _stepsLayout->takeAt(0);
        if (item) {
            if (item->widget()) {
                delete item->widget();  // 立即删除控件
            }
            delete item;  // 删除布局项
        }
    }
    _stepCheckboxes.clear();

    // 如果没有展开，不创建复选框
    if (!_expanded || _stage.steps.isEmpty()) {
        return;
    }

    // 为每个步骤创建复选框
    for (int i = 0; i < _stage.steps.size(); ++i) {
        const QJsonObject &step = _stage.steps[i];
        QString stepTitle = step["step_title"].toString();
        bool stepCompleted = step["completed"].toBool();

        QHBoxLayout *stepLayout = new QHBoxLayout();
        stepLayout->setContentsMargins(15, 2, 5, 2);  // 左缩进显示层级关系
        stepLayout->setSpacing(8);

        QCheckBox *stepCheckbox = new QCheckBox(this);
        stepCheckbox->setChecked(stepCompleted);
        stepCheckbox->setStyleSheet(R"(
            QCheckBox {
                font-size: 13px;
                spacing: 6px;
            }
            QCheckBox::indicator {
                width: 16px;
                height: 16px;
                border: 2px solid #bdc3c7;
                border-radius: 3px;
                background-color: white;
            }
            QCheckBox::indicator:hover {
                border: 2px solid #3498db;
            }
            QCheckBox::indicator:checked {
                background-color: #27ae60;
                border: 2px solid #27ae60;
            }
        )");

        // 使用 userData 存储步骤索引
        stepCheckbox->setProperty("stepIndex", i);
        connect(stepCheckbox, &QCheckBox::stateChanged, this, &StageItem::onStepCheckboxStateChanged);

        QLabel *stepLabel = new QLabel(stepTitle, this);
        stepLabel->setStyleSheet(R"(
            QLabel {
                color: #555;
                font-size: 13px;
            }
        )");
        stepLabel->setWordWrap(true);

        stepLayout->addWidget(stepCheckbox);
        stepLayout->addWidget(stepLabel, 1);

        _stepsLayout->addLayout(stepLayout);
        _stepCheckboxes.append(stepCheckbox);
    }
}

void StageItem::updateDisplay()
{
    // 构建标题文本
    QString icon = _expanded ? "▼" : "▶";
    QString titleText = QString("<a href=\"expand\" style=\"text-decoration:none; color:inherit;\">")
                        + icon + " <b>" + _stage.stage_name + "</b></a>";

    // 完成状态样式
    if (_stage.completed) {
        _label->setStyleSheet(R"(
            QLabel {
                color: #27ae60;
                font-size: 14px;
                padding: 4px 8px;
                border-radius: 4px;
            }
            QLabel:hover {
                background-color: #f8f9fa;
                color: #229954;
            }
        )");
    } else {
        _label->setStyleSheet(R"(
            QLabel {
                color: #2c3e50;
                font-size: 14px;
                padding: 4px 8px;
                border-radius: 4px;
            }
            QLabel:hover {
                background-color: #f8f9fa;
                color: #3498db;
            }
        )");
    }

    _label->setText(titleText);

    // 计算已完成步骤数
    int completedSteps = 0;
    for (const QJsonObject &step : _stage.steps) {
        if (step["completed"].toBool()) {
            completedSteps++;
        }
    }

    // 构建详情文本
    QString detailsText = "   预计: " + QString::number(_stage.estimated_hours) + "小时"
                         + " | " + QString::number(_stage.steps.size()) + "个步骤"
                         + " | 已完成: " + QString::number(completedSteps) + "/" + QString::number(_stage.steps.size());

    _detailsLabel->setText(detailsText);

    // 更新步骤复选框显示
    updateStepsDisplay();

    // 触发大小重新计算，使列表项高度正确更新
    updateGeometry();
}

void StageItem::updateStepsDisplay()
{
    if (_expanded) {
        createStepCheckboxes();
        _stepsContainer->show();
    } else {
        _stepsContainer->hide();
    }
}

void StageItem::onCheckboxStateChanged(int state)
{
    bool completed = (state == Qt::Checked);
    _stage.completed = completed;

    // 更新标签样式（不重新创建步骤UI）
    QString icon = _expanded ? "▼" : "▶";
    QString titleText = QString("<a href=\"expand\" style=\"text-decoration:none; color:inherit;\">")
                        + icon + " <b>" + _stage.stage_name + "</b></a>";

    if (completed) {
        _label->setStyleSheet(R"(
            QLabel {
                color: #27ae60;
                font-size: 14px;
                padding: 4px 8px;
                border-radius: 4px;
            }
            QLabel:hover {
                background-color: #f8f9fa;
                color: #229954;
            }
        )");
    } else {
        _label->setStyleSheet(R"(
            QLabel {
                color: #2c3e50;
                font-size: 14px;
                padding: 4px 8px;
                border-radius: 4px;
            }
            QLabel:hover {
                background-color: #f8f9fa;
                color: #3498db;
            }
        )");
    }

    _label->setText(titleText);

    emit completionChanged(_index, completed);
}

void StageItem::onLabelClicked()
{
    _expanded = !_expanded;
    _stage.expanded = _expanded;

    updateDisplay();

    emit expansionChanged(_index, _expanded);
}

void StageItem::onStepCheckboxStateChanged(int state)
{
    QCheckBox *checkbox = qobject_cast<QCheckBox*>(sender());
    if (!checkbox) return;

    bool completed = (state == Qt::Checked);
    int stepIndex = checkbox->property("stepIndex").toInt();

    // 更新本地数据
    if (stepIndex >= 0 && stepIndex < _stage.steps.size()) {
        // QJsonObject 是不可变的，需要创建新对象来修改
        QJsonObject stepObj = _stage.steps[stepIndex];  // 已经是 QJsonObject
        stepObj["completed"] = completed;
        _stage.steps[stepIndex] = stepObj;
    }

    // 更新详情标签中的完成统计
    updateDisplay();

    emit stepCompletedChanged(_index, stepIndex, completed);
}
