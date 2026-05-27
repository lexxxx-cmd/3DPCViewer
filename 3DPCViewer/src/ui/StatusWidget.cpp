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
    QString compound_key = bag_it.key();
    QStringList parts = compound_key.split("|");
    if (parts.size() < 2) continue; // Safety check

    QString bag_uuid = parts[0];
    QString bag_name = parts[1];

    QStandardItem* bag_item = new QStandardItem(bag_name);
    bag_item->setEditable(false);
    bag_item->setSelectable(false);

    const auto& origins = bag_it.value();
    for (auto origin_it = origins.begin(); origin_it != origins.end(); ++origin_it) {
      QString origin_name = origin_it.key();
      QString display_name = origin_name == "raw" ? "raw (" + origin_name + ")" 
                                                  : "SLAM: " + origin_name;
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
          topic_item->setData(bag_uuid, Qt::UserRole + 2);

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

  bool is_checked = (item->checkState() == Qt::Checked);

  if (is_checked) {
      // 1. Get the current focal properties from this checked item
      QString active_origin = item->data(Qt::UserRole + 1).toString();
      QString active_uuid = item->data(Qt::UserRole + 2).toString();

      // Ensure this item is technically a Topic leaf node
      if (!active_uuid.isEmpty() && !active_origin.isEmpty()) {
          // 2. Perform exclusive uncheck across the entire tree
          topic_model->blockSignals(true);

          for (int b = 0; b < topic_model->rowCount(); ++b) {
              QStandardItem* bag_node = topic_model->item(b);
              if (!bag_node) continue;

              for (int o = 0; o < bag_node->rowCount(); ++o) {
                  QStandardItem* origin_node = bag_node->child(o);
                  if (!origin_node) continue;

                  for (int t = 0; t < origin_node->rowCount(); ++t) {
                      QStandardItem* target_topic = origin_node->child(t);
                      if (!target_topic || target_topic == item) continue;

                      QString target_origin = target_topic->data(Qt::UserRole + 1).toString();
                      QString target_uuid = target_topic->data(Qt::UserRole + 2).toString();

                      // If this topic belongs to a DIFFERENT Bag OR a DIFFERENT Origin group, UNCHECK it!
                      if (target_uuid != active_uuid || target_origin != active_origin) {
                          if (target_topic->checkState() == Qt::Checked) {
                              target_topic->setCheckState(Qt::Unchecked);
                          }
                      }
                  }
              }
          }

          topic_model->blockSignals(false);

          // 3. Inform the DataService to update the backend Focus Pointers for playback
          emit requestSetCurrentDataSource(active_uuid, active_origin);
      }
  } else {
      // Logic for unchecking a node (can be left blank unless full cleanup is needed)
  }
}

bool StatusWidget::checkColmapExportConditions(QString& out_bag_uuid, QString& out_origin_name) {
  if (!topic_model) return false;

  for (int b = 0; b < topic_model->rowCount(); ++b) {
      QStandardItem* bag_node = topic_model->item(b);
      if (!bag_node) continue;

      for (int o = 0; o < bag_node->rowCount(); ++o) {
          QStandardItem* origin_node = bag_node->child(o);
          if (!origin_node) continue;

          bool hasChecked = false;
          bool hasAftMapped = false;
          bool hasCloud = false;

          for (int t = 0; t < origin_node->rowCount(); ++t) {
              QStandardItem* target_topic = origin_node->child(t);
              if (!target_topic) continue;

              if (target_topic->checkState() == Qt::Checked) {
                  hasChecked = true;
                  out_bag_uuid = target_topic->data(Qt::UserRole + 2).toString();
                  out_origin_name = target_topic->data(Qt::UserRole + 1).toString();
              }

              QString topic_text = target_topic->text();
              if (topic_text == "/aft_mapped_to_init") hasAftMapped = true;
              if (topic_text == "/cloud_registered_rgb") hasCloud = true;
          }

          if (hasChecked) {
              return hasAftMapped && hasCloud;
          }
      }
  }

  return false;
}



