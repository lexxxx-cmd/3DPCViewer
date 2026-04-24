#include "Controller.h"
#include <QDebug>
#include <QTimer>
#include <QDateTime>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>
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
  connect(viewer.get(), &PCViewer::requestProcessBin, data_service.get(), &DataService::requestProcessBin);
  connect(viewer.get(), &PCViewer::progressUpdated, data_service.get(), &DataService::updateProgress);

  connect(data_service.get(), &DataService::cloudFrameReady, viewer.get(), &PCViewer::cloudFrameReady);
  connect(data_service.get(), &DataService::imageFrameReady, viewer.get(), &PCViewer::imageFrameReady);
  connect(data_service.get(), &DataService::odomFrameReady, viewer.get(), &PCViewer::odomFrameReady);
  connect(data_service.get(), &DataService::topicListReady, viewer.get(), &PCViewer::topicListReady);
  connect(data_service.get(), &DataService::messageNumReady, viewer.get(), &PCViewer::messageNumReady);

  connect(viewer.get(), &PCViewer::requestRunSlam, this, &Controller::handleRunSlamRequest);
  connect(viewer.get(), &PCViewer::requestExportColmap, this, &Controller::handleExportColmapRequest);
  connect(viewer.get(), &PCViewer::requestSetCurrentDataSource, data_service.get(), &DataService::requestSetCurrentDataSource);

  connect(slam_manager.get(), &slam::SLAMNodeManager::nodeOutputReceived, this, [](const QString& msg){
    qDebug() << "[SLAM Process] :" << msg;
  });
  // 【新增】：捕获并打印 SLAM 进程的标准错误输出 (cerr)
  connect(slam_manager.get(), &slam::SLAMNodeManager::nodeErrorReceived, this, [](const QString& msg) {
      qWarning() << "[SLAM Process Error] :" << msg;
      });
  connect(slam_manager.get(), &slam::SLAMNodeManager::slamResponseReceived, this, &Controller::handleSlamResponse);

  connect(data_service.get(), &DataService::nextSlamFrameReady, this, &Controller::handleNextSlamFrame);
  connect(data_service.get(), &DataService::slamStreamFinished, this, [this]() {
     qDebug() << "SLAM Stream exhausted, sending CMD_FINISH";
     slam_manager->sendFinishCommand({});
  });
}

void Controller::handleExportColmapRequest() {
  QString bag_uuid, origin_name;
  bool ok = viewer->getControlPanel()->getStatusWidget()->checkColmapExportConditions(bag_uuid, origin_name);

  if (!ok) {
    QMessageBox::warning(viewer.get(), "Export Failed", 
        "Cannot export to COLMAP format.\n\n"
        "Please ensure you have selected a data group that contains both:\n"
        "- /aft_mapped_to_init\n"
        "- /cloud_registered_rgb\n"
        "and that at least one of its topics is checked.");
    return;
  }

  QMessageBox::information(viewer.get(), "Export Ready", 
      QString("Ready to export database [%1] - [%2].\n"
              "This string serves as a placeholder for kicking off ColmapExporter.")
              .arg(bag_uuid).arg(origin_name));

  // Task 1.3: Start ColmapExporter process
  QProcess* exporter_process = new QProcess(this);
  QString exe_path = QCoreApplication::applicationDirPath() + "/ColmapExporter.exe";
  // The path depends on where CMake outputs the tools binaries. 
  // We can also try a relative path if it's in the build tree.
  if (!QFileInfo::exists(exe_path)) {
      exe_path = "E:/C++_pj/repos/3DPCViewer/out/build/x64-Release/3DPCViewer/src/tools/colmap_exporter/ColmapExporter.exe";
  }

  QStringList args;
  args << "--zmq-port" << "5567"; // example port

  connect(exporter_process, &QProcess::readyReadStandardOutput, this, [exporter_process]() {
      qDebug() << "[ColmapExporter Out]:" << exporter_process->readAllStandardOutput().trimmed();
  });
  connect(exporter_process, &QProcess::readyReadStandardError, this, [exporter_process]() {
      qDebug() << "[ColmapExporter Err]:" << exporter_process->readAllStandardError().trimmed();
  });
  connect(exporter_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), 
      this, [exporter_process](int exitCode, QProcess::ExitStatus exitStatus) {
      qDebug() << "[ColmapExporter] exited with code" << exitCode;
      exporter_process->deleteLater();
  });

  exporter_process->start(exe_path, args);
  if (!exporter_process->waitForStarted(2000)) {
      qDebug() << "Failed to start ColmapExporter at:" << exe_path;
      QMessageBox::warning(viewer.get(), "Export Failed", "Failed to start ColmapExporter.exe\nPath: " + exe_path);
  } else {
      qDebug() << "Started ColmapExporter successfully. PID:" << exporter_process->processId();
      emit data_service->requestExportColmapStream(bag_uuid, origin_name, 5567);
  }
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
  } else if (algorithm == "ORBSLAM3") {
        QString exe_path = "E:/shixi/dafentech/ORB_SLAM3_Windows/x64/Release/slam.exe";
        QString mode_path = "mono_inertial_tum_vi";
        QString vocab_path = "E:/shixi/dafentech/ORB_SLAM3_Windows/Vocabulary/ORBvoc.bin";
        QString settings_path = "E:/shixi/dafentech/ORB_SLAM3_Windows/Examples/Monocular-Inertial/TUM4.yaml";
        QString working_dir = "E:/shixi/dafentech/ORB_SLAM3_Windows";

        QStringList args;
        args << mode_path << vocab_path << settings_path;

        bool ok = slam_manager->startAlgorithm(exe_path, args, "tcp://127.0.0.1:5555", working_dir);
        if (ok) {
           qDebug() << "ORB-SLAM3 process started! Initializing Real DB Stream and forcing start...";
           bool stream_ok = false;
           QMetaObject::invokeMethod(data_service->getDbManager(), "initSlamStream", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, stream_ok));

           if (stream_ok) {
               QTimer::singleShot(1000, this, [this](){
                 slam_manager->sendStartCommand({QByteArray("RealBagUUID"), QByteArray("ORBSLAM3_CONFIG")});
               });
           } else {
               qDebug() << "Failed to init SQL Stream for ORB-SLAM3!";
               slam_manager->stopAlgorithm();
           }
        } else {
           qDebug() << "Failed to start ORB-SLAM3 executable at:" << exe_path;
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
    if (true || slam_manager->isRunning()) {
        static int test_count = 0;
        if (test_count < 20) {
            qDebug() << "frame:" << test_count 
                     << "timestamp:" << timestamp 
                     << "topic:" << topic 
                     << "payload size:" << payload.size();
            test_count++;
        }

        QByteArray ts_bytes = QString::number(timestamp).toUtf8();
        QByteArray topic_bytes = topic.toUtf8();
        // Using existing payload mechanism; pass ts, topic, payload
        QList<QByteArray> parts = { ts_bytes, topic_bytes, payload };
        slam_manager->sendFrameCommand(parts);
    }
  }
