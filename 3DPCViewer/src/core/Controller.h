#pragma once

#include <QObject>
#include <memory>
#include "ui/PCViewer.h"
#include "DataService.h"
#include "../slam/SLAMNodeManager.h"

class Controller : public QObject {
  Q_OBJECT

 public:
  Controller(QObject* parent);
  ~Controller();

  void run();

 private:
  void setup_connections();

 private slots:
  void handleRunSlamRequest(const QString& algorithm, bool is_rt_preview);
  void handleSlamResponse(slam::net::Command cmd, const QList<QByteArray>& parts);
  void handleMockFrameSend();

 private:
  std::unique_ptr<PCViewer> viewer;
  std::unique_ptr<DataService> data_service;
  std::unique_ptr<slam::SLAMNodeManager> slam_manager;
  
  bool is_rt_preview_enabled_ = false;
  QTimer* mock_timer_ = nullptr;
};
