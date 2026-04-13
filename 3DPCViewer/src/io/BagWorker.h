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
  void updateProgress(const int value);
  void stopProcessing();

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
  std::atomic<bool> stop_flag;
  std::unordered_map<std::string, std::vector<std::vector<uint8_t>>> bag_cache;

  GeneralCloudFrame parseLivoxPayload(const uint8_t* payload, size_t length);
  GeneralCloudFrame parseSensorPc2Payload(const uint8_t* payload, size_t length);
  ImageFrame parseImagePayload(const uint8_t* payload, size_t length);
  OdomFrame parseOdomPayload(const uint8_t* payload, size_t length);
};
