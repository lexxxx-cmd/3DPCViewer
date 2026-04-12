#pragma once

#include <QObject>
#include <QString>
#include <QThread>
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

    // Parse raw messages from DB and emit frame signals
    void parseMessages(const std::vector<RawBagMessage>& messages, int msgIndex);

    void stopProcessing();

signals:
    void cloudFrameReady(const GeneralCloudFrame& frame);
    void imageFrameReady(const ImageFrame& frame);
    void odomFrameReady(const OdomFrame& frame);
    void progressUpdated(int percent);
    void topicListReady(const std::vector<std::string>& topics);
    void messageNumReady(int num);

    // CDR-encoded message ready for DatabaseWorker
    // Also includes msgIndex for DB storage
    void parsedMessageReady(const RawBagMessage& msg, int msgIndex);

    void errorOccur(const QString& errorMsg);

    void finished();

private:
    std::atomic<bool> m_stopFlag;
    int m_currentBagIndex{0};

    GeneralCloudFrame parseLivoxPayload(const uint8_t* payload, size_t length);
    GeneralCloudFrame parseSensorPC2Payload(const uint8_t* payload, size_t length);
    ImageFrame        parseImagePayload(const uint8_t* payload, size_t length);
    OdomFrame         parseOdomPayload(const uint8_t* payload, size_t length);
};
