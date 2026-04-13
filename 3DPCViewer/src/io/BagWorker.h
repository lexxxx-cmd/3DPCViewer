#pragma once

#include <QObject>
#include <QString>
#include <unordered_map>
#include <atomic>
#include "BagDataTypes.h"

class BagWorker : public QObject {
  Q_OBJECT

 public:
  explicit BagWorker(QObject* parent = nullptr);
  ~BagWorker() = default;

 public slots:
  void processBag(const QString& bag_path);
  void updateProgress(const QString& topic_name, const int percent, const QByteArray& payload_data);
  void stopProcessing();

 signals:
  void cloudFrameReady(const GeneralCloudFrame& frame);
  void imageFrameReady(const ImageFrame& frame);
  void odomFrameReady(const OdomFrame& frame);
  void progressUpdated(int percent);
  void topicListReady(const std::vector<std::string>& topics);
  void messageNumReady(int num);
  void errorOccurred(const QString& error_msg);
  void finished();
  
  void topicInfoReady(const QString& bag_uuid, const QString& topic_name, 
                      const QString& msg_type);
  void payloadReady(const QString& topic_name, int msg_index, qint64 timestamp, 
                    const QByteArray& payload);
  void requestPayload(const QString& topic_name, int msg_index);

 private:
  std::atomic<bool> stop_flag;
  
  QString generateUUID();
  GeneralCloudFrame parseLivoxPayload(const uint8_t* payload, size_t length);
  GeneralCloudFrame parseSensorPc2Payload(const uint8_t* payload, size_t length);
  ImageFrame parseImagePayload(const uint8_t* payload, size_t length);
  OdomFrame parseOdomPayload(const uint8_t* payload, size_t length);
};
