#include "DataService.h"
#include <QMetaType>

DataService::DataService(QObject* parent) : QObject(parent) {
  qRegisterMetaType<TopicTreeData>("TopicTreeData");

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

  // DatabaseManager is now the source of truth for the topic list
  connect(db_manager, &DatabaseManager::topicListReady, this, &DataService::topicListReady);
  connect(db_manager, &DatabaseManager::messageNumReady, this, &DataService::messageNumReady);
  connect(db_manager, &DatabaseManager::nextSlamFrameReady, this, &DataService::nextSlamFrameReady);
  connect(db_manager, &DatabaseManager::slamStreamFinished, this, &DataService::slamStreamFinished);

  connect(bag_worker, &BagWorker::topicInfoReady, this, [this](
      const QString& bag_uuid, const QString& topic_name, 
      const QString& msg_type) {
    current_bag_uuid = bag_uuid;
  });

  connect(bag_worker, &BagWorker::batchProcessingFinished, this, [this](int max_size){
    emit requestFinalizeBagProcessing(max_size);
  });

  connect(db_manager, &DatabaseManager::payloadReady, bag_worker, &BagWorker::updateProgress);
  connect(bag_worker, &BagWorker::payloadsBatchReady, this, [this](
      const QString& bag_uuid, const QString& topic_name, const QString& msg_type, 
      const QVariantList& msg_indices, const QVariantList& timestamps, 
      const QVariantList& payloads) {
    emit requestBatchStoreMessages(bag_uuid, "raw", topic_name, msg_type, msg_indices, timestamps, payloads);
  });

  // Connect internal signals to worker slots (replacing invokeMethod)
  connect(this, &DataService::requestInitializeDb, db_manager,
          &DatabaseManager::initialize, Qt::QueuedConnection);
  //TODO
  connect(this, &DataService::requestExportColmapStream, db_manager,
          &DatabaseManager::exportColmapStreamAll, Qt::QueuedConnection);
  connect(this, &DataService::requestInsertTopic, db_manager,
          &DatabaseManager::insertTopic, Qt::QueuedConnection);
  connect(this, &DataService::requestBatchStoreMessages, db_manager,
          &DatabaseManager::batchInsertProcessedFrames, Qt::QueuedConnection);
  connect(this, &DataService::requestFinalizeBagProcessing, db_manager,
          &DatabaseManager::finalizeBagProcessing, Qt::QueuedConnection);
  connect(this, &DataService::requestStoreMessage, db_manager,
          &DatabaseManager::storeMessage, Qt::QueuedConnection);
  connect(this, &DataService::requestUpdateProgress, db_manager,
          &DatabaseManager::updateProgress, Qt::QueuedConnection);
  connect(this, &DataService::requestFetchTopicList, db_manager,
          &DatabaseManager::fetchTopicList, Qt::QueuedConnection);
  connect(this, &DataService::requestFetchNextSlamFrame, db_manager,
          &DatabaseManager::fetchNextSlamFrame, Qt::QueuedConnection);
  connect(this, &DataService::requestSetCurrentDataSource, db_manager,
          &DatabaseManager::setCurrentDataSource, Qt::QueuedConnection);
  connect(this, &DataService::requestProcessBag, bag_worker,
          &BagWorker::processBag, Qt::QueuedConnection);
  connect(this, &DataService::requestProcessBin, bag_worker,
      &BagWorker::processBin, Qt::QueuedConnection);

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
void DataService::startProcessBin(const QString& path) {
    if (bag_worker) {
        emit requestProcessBin(path);
    }
}

void DataService::updateProgress(const int value) {
  if (db_manager) {
    emit requestUpdateProgress(value);
  }
}

void DataService::stopProcess() {
}
