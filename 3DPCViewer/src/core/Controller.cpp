#include "Controller.h"
#include <QDebug>
#include <QTimer>
#include <QDateTime>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include "slam/SlamNetProtocol.h"

Controller::Controller(QObject* parent) : QObject(parent) {
  viewer = std::make_unique<PCViewer>();
  data_service = std::make_unique<DataService>();
  slam_manager = std::make_unique<slam::SLAMNodeManager>();

  mock_timer_ = new QTimer(this);
  mock_timer_->setInterval(100);

  setup_connections();
}

Controller::~Controller() {
}

void Controller::run() {
  viewer->show();
}

void Controller::setup_connections() {
  connect(viewer.get(), &PCViewer::requestProcessBag, data_service.get(), &DataService::startProcess);
  connect(viewer.get(), &PCViewer::progressUpdated, data_service.get(), &DataService::updateProgress);

  connect(data_service.get(), &DataService::cloudFrameReady, viewer.get(), &PCViewer::cloudFrameReady);
  connect(data_service.get(), &DataService::imageFrameReady, viewer.get(), &PCViewer::imageFrameReady);
  connect(data_service.get(), &DataService::odomFrameReady, viewer.get(), &PCViewer::odomFrameReady);
  connect(data_service.get(), &DataService::topicListReady, viewer.get(), &PCViewer::topicListReady);
  connect(data_service.get(), &DataService::messageNumReady, viewer.get(), &PCViewer::messageNumReady);

  connect(viewer.get(), &PCViewer::requestRunSlam, this, &Controller::handleRunSlamRequest);

  connect(slam_manager.get(), &slam::SLAMNodeManager::nodeOutputReceived, this, [](const QString& msg){
    qDebug() << "[SLAM Process] :" << msg;
  });
  
  connect(slam_manager.get(), &slam::SLAMNodeManager::slamResponseReceived, this, &Controller::handleSlamResponse);

  connect(mock_timer_, &QTimer::timeout, this, &Controller::handleMockFrameSend);
}

void Controller::handleRunSlamRequest(const QString& algorithm, bool is_rt_preview) {
  if (slam_manager->isRunning()) {
      qDebug() << "SLAM is already running! Ignoring duplicate request.";
      return;
  }

  qDebug() << "Controller received request to run SLAM:" << algorithm << "RT Preview:" << is_rt_preview;
  is_rt_preview_enabled_ = is_rt_preview;

  if (algorithm == "MockSLAM") {
    bool ok = slam_manager->startAlgorithm("examples/mock_slam/mock_slam_node.exe", QStringList(), "tcp://127.0.0.1:5555");
    if (ok) {
       qDebug() << "Mock SLAM process started alongside ZMQ network worker! Sending CMD_START shortly...";
       
       // Give it a tiny bit of time to bind ZMQ inside the child process. Then send START.
       QTimer::singleShot(1000, this, [this](){
         slam_manager->sendStartCommand({QByteArray("MockBagUUID"), QByteArray("MockConfigXYZ")});
       });
    } else {
       qDebug() << "Failed to start mock_slam.exe";
    }
  } else {
    qDebug() << "Unknown SLAM Algorithm:" << algorithm;
  }
}

void Controller::handleSlamResponse(slam::net::Command cmd, const QList<QByteArray>& parts) {
  qDebug() << "Controller handled SLAM Response -> CMD:" << static_cast<int>(cmd) << "Parts:" << parts.size();

  if (cmd == slam::net::Command::CMD_START && parts.size() > 0 && 
      (QString::fromUtf8(parts[0]) == "OK" || QString::fromUtf8(parts[0]) == "STARTED")) {
     qDebug() << "SLAM started successfully according to response! Beginning simulated frame push...";
     // Stop fixed timer and use Ping-Pong (one frame at a time)
     if (mock_timer_) mock_timer_->stop();
     QTimer::singleShot(10, this, &Controller::handleMockFrameSend);
  } 
  else if (cmd == slam::net::Command::CMD_FRAME) {
     qDebug() << "Got processed frame result from Mock SLAM.";
     static int count = 0;
     count++;
     if (count >= 20) {
        qDebug() << "Sent 20 frames, stopping simulation and sending CMD_FINISH";
        slam_manager->sendFinishCommand({});
        count = 0;
     } else {
        // Ping-pong flow: Fire next mock frame only AFTER we get current frame's response
        QTimer::singleShot(10, this, &Controller::handleMockFrameSend);
     }

     if (is_rt_preview_enabled_ && parts.size() > 1) {
        // Just acknowledging that real-time preview would be intercepted here
        qDebug() << "RT Preview is ENABLED, intercepting payload for visualization:" << parts.size() << "parts provided.";
     }
  }
  else if (cmd == slam::net::Command::CMD_FINISH) {
     qDebug() << "Mock SLAM finished normally!";
     slam_manager->stopAlgorithm();
  }
}

void Controller::handleMockFrameSend() {
  if (slam_manager->isRunning()) {
      QByteArray mockPcData("MockPointCloudBlob");
      QByteArray mockTimestamp(QString::number(QDateTime::currentMSecsSinceEpoch()).toUtf8());
      QList<QByteArray> payload = { mockTimestamp, mockPcData };
      slam_manager->sendFrameCommand(payload);
  } else {
      mock_timer_->stop();
  }
}
