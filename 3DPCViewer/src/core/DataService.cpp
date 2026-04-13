#include "DataService.h"

DataService::DataService(QObject* parent) : QObject(parent) {
  worker_thread = new QThread(this);
  bag_worker = new BagWorker();
  bag_worker->moveToThread(worker_thread);

  connect(worker_thread, &QThread::finished, bag_worker, &QObject::deleteLater);
  connect(bag_worker, &QObject::destroyed, [this]() {
    bag_worker = nullptr;
  });

  connect(bag_worker, &BagWorker::cloudFrameReady, this, &DataService::cloudFrameReady);
  connect(bag_worker, &BagWorker::imageFrameReady, this, &DataService::imageFrameReady);
  connect(bag_worker, &BagWorker::odomFrameReady, this, &DataService::odomFrameReady);
  connect(bag_worker, &BagWorker::progressUpdated, this, &DataService::progressUpdated);
  connect(bag_worker, &BagWorker::topicListReady, this, &DataService::topicListReady);
  connect(bag_worker, &BagWorker::messageNumReady, this, &DataService::messageNumReady);

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
  if (bag_worker) {
    QMetaObject::invokeMethod(bag_worker, "processBag",
                              Qt::QueuedConnection,
                              Q_ARG(QString, path));
  }
}

void DataService::updateProgress(const int value) {
  if (bag_worker) {
    QMetaObject::invokeMethod(bag_worker, "updateProgress",
                              Qt::QueuedConnection,
                              Q_ARG(int, value));
  }
}

void DataService::stopProcess() {
}

