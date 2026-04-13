#pragma once

#include <QObject>
#include <QThread>
#include "io/BagWorker.h"

class DataService : public QObject {
  Q_OBJECT

 public:
  DataService(QObject* parent = nullptr);
  ~DataService();

  BagWorker* getWorker() const { return bag_worker; }
  QThread* getThread() const { return worker_thread; }

 public slots:
  void startProcess(const QString& path);
  void updateProgress(const int value);
  void stopProcess();

 signals:
  void cloudFrameReady(const GeneralCloudFrame& frame);
  void imageFrameReady(const ImageFrame& frame);
  void odomFrameReady(const OdomFrame& frame);
  void progressUpdated(int percent);
  void topicListReady(const std::vector<std::string>& topics);
  void messageNumReady(int num);
  void errorOccur(const QString& error_msg);
  void finished();

 private:
  BagWorker* bag_worker;
  QThread* worker_thread;
};
