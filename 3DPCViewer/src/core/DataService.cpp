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
    emit requestInsertTopic(bag_uuid, topic_name, msg_type);
  });
  
  connect(db_manager, &DatabaseManager::payloadReady, bag_worker, &BagWorker::updateProgress);
  connect(bag_worker, &BagWorker::payloadReady, this, [this](
      const QString& topic_name, int msg_index, qint64 timestamp, 
      const QByteArray& payload) {
    emit requestStoreMessage(current_bag_uuid, topic_name, msg_index, timestamp, payload);
  });

  // Connect internal signals to worker slots (replacing invokeMethod)
  connect(this, &DataService::requestInitializeDb, db_manager,
          &DatabaseManager::initialize, Qt::QueuedConnection);
  connect(this, &DataService::requestInsertTopic, db_manager,
          &DatabaseManager::insertTopic, Qt::QueuedConnection);
  connect(this, &DataService::requestStoreMessage, db_manager,
          &DatabaseManager::storeMessage, Qt::QueuedConnection);
  connect(this, &DataService::requestUpdateProgress, db_manager,
          &DatabaseManager::updateProgress, Qt::QueuedConnection);
  connect(this, &DataService::requestProcessBag, bag_worker,
          &BagWorker::processBag, Qt::QueuedConnection);

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
    emit requestInitializeDb(path);
    emit requestProcessBag(path);
  }
}

void DataService::updateProgress(const int value) {
  if (db_manager) {
    emit requestUpdateProgress(value);
  }
}

void DataService::stopProcess() {
}
