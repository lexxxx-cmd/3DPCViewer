#pragma once

#include <QObject>
#include <QThread>
#include "io/BagWorker.h"
#include "io/DatabaseManager.h"

class DataService : public QObject {
  Q_OBJECT

 public:
  DataService(QObject* parent = nullptr);
  ~DataService();

  BagWorker* getWorker() const { return bag_worker; }
  QThread* getThread() const { return worker_thread; }
  DatabaseManager* getDbManager() const { return db_manager; }

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
  void errorOccurred(const QString& error_msg);
  void finished();

  // Internal signals for cross-thread communication (replacing invokeMethod)
  void requestInitializeDb(const QString& bag_path);
  void requestInsertTopic(const QString& bag_uuid, const QString& topic_name,
                          const QString& msg_type);
  void requestStoreMessage(const QString& bag_uuid, const QString& topic_name,
                           int msg_index, qint64 timestamp,
                           const QByteArray& payload);
  void requestUpdateProgress(int percent);
  void requestProcessBag(const QString& path);

 private:
  BagWorker* bag_worker;
  QThread* worker_thread;
  DatabaseManager* db_manager;
  QString current_bag_uuid;
};
