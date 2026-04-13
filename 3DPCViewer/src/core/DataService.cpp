#include "DataService.h"

DataService::DataService(QObject* parent) : QObject(parent) {
  worker_thread = new QThread(this);
  bag_worker = new BagWorker();
  db_manager = new DatabaseManager();
  
  bag_worker->moveToThread(worker_thread);
  db_manager->moveToThread(worker_thread);

  connect(worker_thread, &QThread::finished, bag_worker, &QObject::deleteLater);
  connect(worker_thread, &QThread::finished, db_manager, &QObject::deleteLater);
  connect(bag_worker, &QObject::destroyed, [this]() {
    bag_worker = nullptr;
  });
  connect(db_manager, &QObject::destroyed, [this]() {
    db_manager = nullptr;
  });

  connect(bag_worker, &BagWorker::cloudFrameReady, this, &DataService::cloudFrameReady);
  connect(bag_worker, &BagWorker::imageFrameReady, this, &DataService::imageFrameReady);
  connect(bag_worker, &BagWorker::odomFrameReady, this, &DataService::odomFrameReady);
  connect(bag_worker, &BagWorker::progressUpdated, this, &DataService::progressUpdated);
  connect(bag_worker, &BagWorker::topicListReady, this, &DataService::topicListReady);
  connect(bag_worker, &BagWorker::messageNumReady, this, &DataService::messageNumReady);
  
  connect(bag_worker, &BagWorker::topicInfoReady, this, [this](
      const QString& bag_uuid, const QString& topic_name, 
      const QString& msg_type) {
    current_bag_uuid = bag_uuid;
    
    QMetaObject::invokeMethod(db_manager, "insertTopic", Qt::QueuedConnection,
        Q_ARG(QString, bag_uuid), Q_ARG(QString, topic_name), 
        Q_ARG(QString, msg_type));
  });
  
  connect(db_manager, &DatabaseManager::payloadReady, bag_worker, &BagWorker::updateProgress);
  connect(bag_worker, &BagWorker::payloadReady, this, [this](
      const QString& topic_name, int msg_index, qint64 timestamp, 
      const QByteArray& payload) {
    QMetaObject::invokeMethod(db_manager, "storeMessage", Qt::QueuedConnection,
        Q_ARG(QString, current_bag_uuid), Q_ARG(QString, topic_name),
        Q_ARG(int, msg_index), Q_ARG(qint64, timestamp), Q_ARG(QByteArray, payload));
  });

  if (!worker_thread->isRunning()) {
    worker_thread->start();
  }
}

DataService::~DataService() {
  if (bag_worker) {
    bag_worker->stopProcessing();
  }
  worker_thread->quit();
  worker_thread->wait();
}

void DataService::startProcess(const QString& path) {
  if (bag_worker && db_manager) {
    QMetaObject::invokeMethod(db_manager, "initialize", Qt::QueuedConnection,
        Q_ARG(QString, path));
    
    QMetaObject::invokeMethod(bag_worker, "processBag",
                              Qt::QueuedConnection,
                              Q_ARG(QString, path));
  }
}

void DataService::updateProgress(const int value) {
  if (db_manager) {
    QMetaObject::invokeMethod(db_manager, "updateProgress",
                              Qt::QueuedConnection,
                              Q_ARG(int, value));
  }
}

void DataService::stopProcess() {
}

