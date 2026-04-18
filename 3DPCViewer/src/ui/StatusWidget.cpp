#include "ui/StatusWidget.h"

StatusWidget::StatusWidget(QWidget* parent) : QWidget(parent), topic_model(nullptr) {
  ui = std::make_unique<Ui::StatusWidgetClass>();
  ui->setupUi(this);
}

StatusWidget::~StatusWidget() = default;

void StatusWidget::onUpdateTopicList(const TopicTreeData& all_topics) {
  if (!topic_model) {
    topic_model = new QStandardItemModel(this);
    topic_model->setHorizontalHeaderLabels({ "Topics View" });

    connect(topic_model, &QStandardItemModel::itemChanged,
        this, &StatusWidget::onTopicStateChanged);
  } else {
    topic_model->clear();
    topic_model->setHorizontalHeaderLabels({ "Topics View" });
  }

  topic_model->blockSignals(true);

  for (auto bag_it = all_topics.begin(); bag_it != all_topics.end(); ++bag_it) {
    QString bag_name = bag_it.key();
    QStandardItem* bag_item = new QStandardItem(bag_name);
    bag_item->setEditable(false);
    bag_item->setSelectable(false);

    const auto& origins = bag_it.value();
    for (auto origin_it = origins.begin(); origin_it != origins.end(); ++origin_it) {
      QString origin_name = origin_it.key();
      QString display_name = origin_name == "raw" ? "原始传感器数据 (" + origin_name + ")" 
                                                  : "SLAM 结果: " + origin_name;
      QStandardItem* group_item = new QStandardItem(display_name);
      group_item->setEditable(false);
      group_item->setSelectable(false);

      for (const QString& topic : origin_it.value()) {
          QStandardItem* topic_item = new QStandardItem(topic);
          topic_item->setCheckable(true);
          topic_item->setCheckState(Qt::Unchecked);
          topic_item->setEditable(false);

          // Store metadata
          topic_item->setData(origin_name, Qt::UserRole + 1);
          topic_item->setData(bag_name, Qt::UserRole + 2);

          group_item->appendRow(topic_item);
      }

      bag_item->appendRow(group_item);
    }

    topic_model->appendRow(bag_item);
  }

  topic_model->blockSignals(false);
  ui->treeView->setModel(topic_model);
  ui->treeView->expandAll();
}

void StatusWidget::onTopicStateChanged(QStandardItem* item) {
  if (!item) return;

  QString topic_name = item->text();
  bool is_checked = (item->checkState() == Qt::Checked);

  if (is_checked) {

  } else {

  }
}



