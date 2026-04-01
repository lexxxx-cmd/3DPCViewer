#pragma once

#include <QObject>
#include <QString>
#include <QThread>
#include <unordered_map>
#include "BagDataTypes.h"

class BagWorker : public QObject
{
	Q_OBJECT

public:
	explicit BagWorker(QObject *parent = nullptr);
	~BagWorker() = default;

public slots:
    // 后台解包的入口槽函数
    void processBag(const QString& bagPath);

    // 用于安全中止读取
    void stopProcessing();

signals:
    // 将解析好的数据抛给前端
    void cloudFrameReady(const LivoxCloudFrame& frame);
    void imageFrameReady(const ImageFrame& frame);
    void odomFrameReady(const OdomFrame& frame);
    void progressUpdated(int percent);
    void topicListReady(const std::vector<std::string>& topics);

    // 错误
    void errorOccur(const QString& errorMsg);

    // 任务结束信号
    void finished();

private:
    std::atomic<bool> m_stopFlag;
    std::unordered_map<std::string, std::vector<std::vector<uint8_t>>> m_bagCache; // 缓存已解析的数据，按topic索引

    // 二进制剥离核心算法
    LivoxCloudFrame parseLivoxPayload(const uint8_t* payload, size_t length);
    ImageFrame parseImagePayload(const uint8_t* payload, size_t length);
    OdomFrame parseOdomPayload(const uint8_t* payload, size_t length);
};
