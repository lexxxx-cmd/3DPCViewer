#pragma once

#include <QObject>
#include <memory>
#include "ui/PCViewer.h"
#include "DataService.h"

class Controller : public QObject {
  Q_OBJECT

 public:
  Controller(QObject* parent);
  ~Controller();

  void run();

 private:
  void setup_connections();

  std::unique_ptr<PCViewer> viewer;
  std::unique_ptr<DataService> data_service;
};
