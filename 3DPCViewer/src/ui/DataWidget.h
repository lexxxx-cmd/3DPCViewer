#pragma once

#include <QWidget>
#include <memory>
#include "ui_DataWidget.h"

QT_BEGIN_NAMESPACE
namespace Ui { class DataWidgetClass; };
QT_END_NAMESPACE

class DataWidget : public QWidget {
  Q_OBJECT

 public:
  DataWidget(QWidget* parent = nullptr);
  ~DataWidget();

 signals:
  void requestProcBag(const QString& bag_path);

 private:
  std::unique_ptr<Ui::DataWidgetClass> ui;
};

