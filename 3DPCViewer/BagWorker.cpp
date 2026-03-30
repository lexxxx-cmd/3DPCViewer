#include "BagWorker.h"
#include "third_party/ros1bag_reader/core/rosbag.h"
#include <QDebug>
#include <QImage>

BagWorker::BagWorker(QObject* parent) : QObject(parent), m_stopFlag(false) {}

void BagWorker::stopProcessing() {
    m_stopFlag = true;
}

void BagWorker::processBag(const QString& bagPath) {
    m_stopFlag = false;

    // 1. 加载 Bag 索引
    Rosbag bag(bagPath.toStdString());

    // 2. 拿到纯粹的二进制大数组
    auto livox_payloads = bag.getRawPayloads("/livox/lidar");
    auto image_payloads = bag.getRawPayloads("/usb_cam/image_raw/compressed");

    int totalFrames = livox_payloads.size();
    if (totalFrames == 0) {
        emit finished();
        return;
    }

    // 3. 开始遍历解析（这里以 Livox 为主线，实际应用可根据时间戳合并播放）
    for (int i = 0; i < totalFrames; ++i) {
        if (m_stopFlag) break;

        // --- 解析 Livox 点云 ---
        const auto& cloud_payload = livox_payloads[i];
        LivoxCloudFrame cloudFrame = parseLivoxPayload(cloud_payload.data(), cloud_payload.size());
        emit cloudFrameReady(cloudFrame);

        // --- 解析对应的图像 (为了演示，假设图像和点云数量一致或近似) ---
        //if (i < image_payloads.size()) {
        //    const auto& img_payload = image_payloads[i];
        //    ImageFrame imgFrame = parseImagePayload(img_payload.data(), img_payload.size());
        //    emit imageFrameReady(imgFrame);
        //}

        emit progressUpdated((i * 100) / totalFrames);

        // 控制播放帧率，比如 10Hz (100ms)
        QThread::msleep(100);
    }

    emit finished();
}

// ----------------- 二进制解析算法 ----------------- //

LivoxCloudFrame BagWorker::parseLivoxPayload(const uint8_t* payload, size_t length) {
    LivoxCloudFrame frame;
    size_t offset = 0;

    // 1. 跨过 std_msgs/Header
    offset += 12; // seq(4) + stamp_sec(4) + stamp_nsec(4)
    uint32_t frame_id_len = *(uint32_t*)(payload + offset); offset += 4;
    frame.frame_id = QString::fromUtf8((const char*)(payload + offset), frame_id_len);
    offset += frame_id_len;

    // 2. 跨过 Livox CustomMsg Header
    frame.timestamp = *(uint64_t*)(payload + offset); offset += 8; // timebase
    uint32_t point_num = *(uint32_t*)(payload + offset); offset += 4; // point_num
    offset += 1; // lidar_id
    offset += 3; // rsvd

    // 3. 解析动态数组长度
    uint32_t array_elements = *(uint32_t*)(payload + offset); offset += 4;

    if (offset + point_num * sizeof(LivoxPoint) > length) {
        qWarning() << "严重：此帧 payload 数据损坏或不完整，丢弃！";
        return frame; // 返回空帧
    }

    // 4. 暴力内存拷贝！(极其快速)
    frame.points.resize(point_num);
    std::memcpy(frame.points.data(), payload + offset, point_num * sizeof(LivoxPoint));

    return frame;
}

ImageFrame BagWorker::parseImagePayload(const uint8_t* payload, size_t length) {
    ImageFrame frame;
    size_t offset = 0;

    // 1. 跨过 Header
    offset += 12;
    uint32_t frame_id_len = *(uint32_t*)(payload + offset); offset += 4;
    offset += frame_id_len;

    // 2. 跨过 format 字符串
    uint32_t format_len = *(uint32_t*)(payload + offset); offset += 4;
    offset += format_len;

    // 3. 读取图像数据长度
    uint32_t data_len = *(uint32_t*)(payload + offset); offset += 4;
    if (offset + data_len > length) {
        return frame; // 越界保护
    }
    // 4. Qt 原生解压 JPEG/PNG
    frame.image.loadFromData(payload + offset, data_len);

    return frame;
}