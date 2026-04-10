#pragma once

#include <QObject>
#include <QString>
#include <QThread>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include "BagDataTypes.h"

class BagWorker : public QObject
{
Q_OBJECT

public:
explicit BagWorker(QObject *parent = nullptr);
~BagWorker() = default;

    void rebuildCacheFromDbMessages(const std::vector<RawBagMessage>& messages, int bagIndex);

public slots:
    // bagIndex: 1-based counter supplied by DataService to distinguish imports
    void processBag(const QString& bagPath, int bagIndex);

    void updateProgress(const int value);
    void setActiveTopics(const std::vector<std::string>& rawTopics);
    void stopProcessing();

signals:
    void cloudFrameReady(const GeneralCloudFrame& frame);
    void imageFrameReady(const ImageFrame& frame);
    void odomFrameReady(const OdomFrame& frame);
    void progressUpdated(int percent);
    void topicListReady(const std::vector<std::string>& topics);
    void messageNumReady(int num);

    // CDR-encoded message ready for DatabaseWorker
    void parsedMessageReady(const RawBagMessage& msg);

    void errorOccur(const QString& errorMsg);

    void finished();

private:
    void clearCurrentCache();

    std::atomic<bool> m_stopFlag;
    mutable std::mutex m_activeTopicsMutex;
    std::unordered_set<std::string> m_activeTopics;
    int m_currentBagIndex{0};
    std::unordered_map<std::string, std::vector<std::vector<uint8_t>>> m_bagCache;
    std::unordered_map<std::string, std::vector<int64_t>>              m_bagTimestamps;
    std::unordered_map<std::string, std::string>                       m_bagTopicTypes;

    GeneralCloudFrame parseLivoxPayload(const uint8_t* payload, size_t length);
    GeneralCloudFrame parseSensorPC2Payload(const uint8_t* payload, size_t length);
    ImageFrame        parseImagePayload(const uint8_t* payload, size_t length);
    OdomFrame         parseOdomPayload(const uint8_t* payload, size_t length);
};
