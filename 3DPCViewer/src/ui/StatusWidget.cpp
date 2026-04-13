#include "ui/StatusWidget.h"

StatusWidget::StatusWidget(QWidget* parent) : QWidget(parent), topic_model(nullptr) {
  ui = std::make_unique<Ui::StatusWidgetClass>();
  ui->setupUi(this);
}

StatusWidget::~StatusWidget() = default;

void StatusWidget::onUpdateTopicList(const std::vector<std::string>& topics) {
  if (!topic_model) {
    topic_model = new QStandardItemModel(this);
    topic_model->setHorizontalHeaderLabels({ "Topic Name" });

    connect(topic_model, &QStandardItemModel::itemChanged,
        this, &StatusWidget::onTopicStateChanged);
  } else {
    topic_model->clear();
    topic_model->setHorizontalHeaderLabels({ "Topic Name" });
  }

  topic_model->blockSignals(true);

  for (const auto& topic : topics) {
    QStandardItem* item = new QStandardItem(QString::fromStdString(topic));

    item->setCheckable(true);
    item->setCheckState(Qt::Unchecked);
    item->setEditable(false);

    topic_model->appendRow(item);
  }

  topic_model->blockSignals(false);
  ui->treeView->setModel(topic_model);
}

void StatusWidget::onTopicStateChanged(QStandardItem* item) {
  if (!item) return;

  QString topic_name = item->text();
  bool is_checked = (item->checkState() == Qt::Checked);

  if (is_checked) {

  } else {

  }
}



