#pragma once

#include <QWidget>
#include <memory>
#include <QStandardItemModel>
#include <QMap>
#include <QStringList>
#include "io/BagDataTypes.h"
#include "ui_StatusWidget.h"

QT_BEGIN_NAMESPACE
namespace Ui { class StatusWidgetClass; };
QT_END_NAMESPACE

class StatusWidget : public QWidget {
  Q_OBJECT

 public:
  StatusWidget(QWidget* parent = nullptr);
  ~StatusWidget();

 public slots:
  void onUpdateTopicList(const TopicTreeData& topics);
  void onTopicStateChanged(QStandardItem* item);

  public:
   bool checkColmapExportConditions(QString& out_bag_uuid, QString& out_origin_name);

 signals:
  void requestSetCurrentDataSource(const QString& bag_uuid, const QString& origin_name);

 private:
  std::unique_ptr<Ui::StatusWidgetClass> ui;
  QStandardItemModel* topic_model;
};

