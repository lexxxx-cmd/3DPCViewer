#include "ui/StatusWidget.h"

StatusWidget::StatusWidget(QWidget *parent)
	: QWidget(parent), topicModel(nullptr)
{
	ui = std::make_unique< Ui::StatusWidgetClass>();
	ui->setupUi(this);
}

StatusWidget::~StatusWidget() = default;

void StatusWidget::onUpdateTopicList(const std::vector<std::string>& topics) {
    // 1. 初始化或清空模型
    if (!topicModel) {
        topicModel = new QStandardItemModel(this);
        topicModel->setHorizontalHeaderLabels({ "Topic Name" });

        // 绑定信号：当 Item 的勾选状态变化时触发
        connect(topicModel, &QStandardItemModel::itemChanged,
            this, &StatusWidget::onTopicStateChanged);
    }
    else {
        topicModel->clear();
        topicModel->setHorizontalHeaderLabels({ "Topic Name" });
    }

    // 2. 阻塞信号（防止在添加大量数据时频繁触发 itemChanged 槽函数）
    topicModel->blockSignals(true);

    // 3. 填充数据
    for (const auto& topic : topics) {
        QStandardItem* item = new QStandardItem(QString::fromStdString(topic));

        // 核心步：设置为可选并赋予初始状态
        item->setCheckable(true);
        item->setCheckState(Qt::Unchecked); // 默认不勾选
        item->setEditable(false);           // 禁止双击修改文字

        topicModel->appendRow(item);
    }

    // 4. 恢复信号并关联视图
    topicModel->blockSignals(false);
    ui->treeView->setModel(topicModel);
}

void StatusWidget::onTopicStateChanged(QStandardItem* item) {
    if (!item) return;

    QString topicName = item->text();
    bool isChecked = (item->checkState() == Qt::Checked);

    if (isChecked) {
        // 执行“开启话题订阅”或“标记记录”逻辑
       
    }
    else {
       
    }
}


