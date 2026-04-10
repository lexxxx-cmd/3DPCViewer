#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QThread>
#include <unordered_map>
#include <unordered_set>
#include "BagDataTypes.h"

class BagWorker : public QObject
{
Q_OBJECT

public:
explicit BagWorker(QObject *parent = nullptr);
~BagWorker() = default;

public slots:
    // bagIndex: 1-based counter supplied by DataService to distinguish imports
    void processBag(const QString& bagPath, int bagIndex);

    void updateProgress(const int value);
    void stopProcessing();

    // Set which raw topic names (e.g. "/livox/lidar") should be visualized.
    // Frames for topics not in this set are silently skipped in updateProgress.
    void setActiveTopics(const QStringList& rawTopics);

    // Replace the in-memory cache with data loaded from the database.
    void receiveBagCache(const BagCacheData& cache);

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
    // Emit the appropriate visualization frame for one raw payload based on
    // its ROS message type name.
    void emitFrameForPayload(const std::string& topic,
                             const std::string& typeName,
                             const std::vector<uint8_t>& payload,
                             int index);

    std::atomic<bool> m_stopFlag;
    std::unordered_map<std::string, std::vector<std::vector<uint8_t>>> m_bagCache;
    std::unordered_map<std::string, std::vector<int64_t>>              m_bagTimestamps;
    std::unordered_map<std::string, std::string>                       m_bagTopicTypes;

    // Raw topic names that the user has checked for visualization.
    // Empty means "show all".
    std::unordered_set<std::string> m_activeTopics;

    GeneralCloudFrame parseLivoxPayload(const uint8_t* payload, size_t length);
    GeneralCloudFrame parseSensorPC2Payload(const uint8_t* payload, size_t length);
    ImageFrame        parseImagePayload(const uint8_t* payload, size_t length);
    OdomFrame         parseOdomPayload(const uint8_t* payload, size_t length);
};
