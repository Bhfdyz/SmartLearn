#ifndef STAGEITEM_H
#define STAGEITEM_H

#include <QWidget>
#include <QCheckBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QVector>
#include "learningpathpage.h"

class StageItem : public QWidget
{
    Q_OBJECT

public:
    explicit StageItem(const PathStage &stage, int index, QWidget *parent = nullptr);

    QSize sizeHint() const override;

    bool isCompleted() const { return _checkbox->isChecked(); }
    void setCompleted(bool completed) { _checkbox->setChecked(completed); }
    bool isExpanded() const { return _expanded; }
    void setExpanded(bool expanded);

    PathStage stageData() const { return _stage; }
    void updateStageData(const PathStage &stage);

    // 新增：设置步骤完成状态（用于从服务器加载）
    void setStepCompleted(int stepIndex, bool completed);
    // 设置所有步骤完成状态（不重新创建UI）
    void setAllStepsCompleted(bool completed);

signals:
    void completionChanged(int index, bool completed);      // 阶段完成状态变化
    void expansionChanged(int index, bool expanded);        // 阶段展开/收起
    void stepCompletedChanged(int stageIndex, int stepIndex, bool completed);  // 步骤完成状态变化

private slots:
    void onCheckboxStateChanged(int state);
    void onLabelClicked();
    void onStepCheckboxStateChanged(int state);  // 步骤复选框状态变化

private:
    void updateDisplay();
    void updateStepsDisplay();  // 新增：更新步骤显示
    void createStepCheckboxes();  // 新增：创建步骤复选框

    int _index;
    PathStage _stage;
    bool _expanded;
    QCheckBox *_checkbox;
    QLabel *_label;
    QLabel *_detailsLabel;  // 保留用于显示"预计X小时 | X个步骤"
    QWidget *_stepsContainer;  // 新增：步骤容器
    QVBoxLayout *_stepsLayout;  // 新增：步骤布局
    QVector<QCheckBox*> _stepCheckboxes;  // 新增：步骤复选框数组
};

#endif // STAGEITEM_H
