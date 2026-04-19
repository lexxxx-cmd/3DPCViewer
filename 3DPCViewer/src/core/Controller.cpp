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
  connect(viewer.get(), &PCViewer::requestSetCurrentDataSource, data_service.get(), &DataService::requestSetCurrentDataSource);

  connect(slam_manager.get(), &slam::SLAMNodeManager::nodeOutputReceived, this, [](const QString& msg){
    qDebug() << "[SLAM Process] :" << msg;
  });
  
  connect(slam_manager.get(), &slam::SLAMNodeManager::slamResponseReceived, this, &Controller::handleSlamResponse);

  connect(data_service.get(), &DataService::nextSlamFrameReady, this, &Controller::handleNextSlamFrame);
  connect(data_service.get(), &DataService::slamStreamFinished, this, [this]() {
     qDebug() << "SLAM Stream exhausted, sending CMD_FINISH";
     slam_manager->sendFinishCommand({});
  });
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
       qDebug() << "Mock SLAM process started! Initializing Real DB Stream and forcing start...";
       bool stream_ok = false;
       QMetaObject::invokeMethod(data_service->getDbManager(), "initSlamStream", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, stream_ok));

       if (stream_ok) {
           QTimer::singleShot(1000, this, [this](){
             slam_manager->sendStartCommand({QByteArray("RealBagUUID"), QByteArray("MockConfigXYZ")});
           });
       } else {
           qDebug() << "Failed to init SQL Stream for SLAM!";
           slam_manager->stopAlgorithm();
       }
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
       qDebug() << "SLAM started successfully according to response! Beginning REAL frame push...";
       // Fire the first request for real payload
       emit data_service->requestFetchNextSlamFrame();
    } 
    else if (cmd == slam::net::Command::CMD_FRAME) {
       // Ping-pong flow: Fire next real frame only AFTER we get current frame's response
       QTimer::singleShot(1, this, [this]() {
           emit data_service->requestFetchNextSlamFrame();
       });

       if (is_rt_preview_enabled_ && parts.size() > 1) {
          // Just acknowledging that real-time preview would be intercepted here
          // qDebug() << "RT Preview is ENABLED, intercepting payload for visualization:" << parts.size() << "parts provided.";
       }
    }
    else if (cmd == slam::net::Command::CMD_FINISH) {
       qDebug() << "SLAM node finished normally!";
       slam_manager->stopAlgorithm();
       QMetaObject::invokeMethod(data_service->getDbManager(), "stopSlamStream", Qt::QueuedConnection);
    }
  }

  void Controller::handleNextSlamFrame(const QString& topic, const QByteArray& payload, qint64 timestamp) {
    if (slam_manager->isRunning()) {
        static int test_count = 0;
        if (test_count < 20) {
            qDebug() << "frame:" << test_count 
                     << "timestamp:" << timestamp 
                     << "topic:" << topic 
                     << "payload size:" << payload.size();
            test_count++;
        }

        QByteArray ts_bytes = QString::number(timestamp).toUtf8();
        // Using existing payload mechanism; usually we'd pass topic, ts, payload
        // For now pass ts and full payload block 
        QList<QByteArray> parts = { ts_bytes, payload };
        slam_manager->sendFrameCommand(parts);
    }
  }
