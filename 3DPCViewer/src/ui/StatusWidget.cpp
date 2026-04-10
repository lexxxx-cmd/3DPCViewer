#include "ui/StatusWidget.h"

StatusWidget::StatusWidget(QWidget *parent)
	: QWidget(parent), topicModel(nullptr)
{
	ui = std::make_unique< Ui::StatusWidgetClass>();
	ui->setupUi(this);
}

StatusWidget::~StatusWidget() = default;

void StatusWidget::onUpdateTopicList(const std::vector<std::string>& topics) {
    if (!topicModel) {
        topicModel = new QStandardItemModel(this);
        topicModel->setHorizontalHeaderLabels({ "Topic Name" });

        
        connect(topicModel, &QStandardItemModel::itemChanged,
            this, &StatusWidget::onTopicStateChanged);
    }

    for (const auto& topic : topics) {
        const QString topicName = QString::fromStdString(topic);
        if (m_knownTopics.contains(topicName)) {
            continue;
        }

        QStandardItem* item = new QStandardItem(topicName);

        item->setCheckable(true);
        item->setCheckState(Qt::Unchecked);
        item->setEditable(false);

        topicModel->appendRow(item);
        m_knownTopics.insert(topicName);
    }

    ui->treeView->setModel(topicModel);
}

void StatusWidget::onTopicStateChanged(QStandardItem* item) {
    if (!item) return;

    QString topicName = item->text();
    bool isChecked = (item->checkState() == Qt::Checked);

    if (isChecked) {
       
    }
    else {
       
    }
}
