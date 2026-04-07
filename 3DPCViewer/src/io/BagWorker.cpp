#include "BagWorker.h"
#include "core/rosbag.h"
#include <QDebug>
#include <QImage>

BagWorker::BagWorker(QObject* parent) : QObject(parent), m_stopFlag(false) {}

void BagWorker::stopProcessing() {
    m_stopFlag = true;
}

void BagWorker::processBag(const QString& bagPath) {
    m_stopFlag = false;

    // 加载 Bag 索引
    Rosbag bag(bagPath.toStdString());
    std::vector<std::string> topicList = bag.getAvailableTopics();

    emit topicListReady(topicList);

    // 拿到纯粹的二进制大数组
    int max_size = 0;
    for (const std::string&topic : topicList) {
		qDebug() << "Bag 中可用 Topic:" << QString::fromStdString(topic);
        if (m_bagCache.find(topic) != m_bagCache.end()) {
			// TODO
            m_bagCache.clear();
			return;
		}
        m_bagCache[topic] = bag.getRawPayloads(topic);
        max_size = std::max(max_size, (int)m_bagCache[topic].size());
	}
    // 保证拿到的最大消息数不为零，否则直接结束
    if (max_size == 0) {
        emit finished();
        return;
	}
    emit messageNumReady(max_size);
    emit finished();
}

void BagWorker::updateProgress(const int value) {
    for (const auto& topicItem : m_bagCache) {
        if (value < m_bagCache[topicItem.first].size()) {
            const auto& payload = m_bagCache[topicItem.first][value];
            if (topicItem.first == "/livox/lidar") {
                GeneralCloudFrame frame = parseLivoxPayload(payload.data(), payload.size());
                emit cloudFrameReady(frame);
            }
            else if (topicItem.first == "/usb_cam/image_raw/compressed") {
                ImageFrame frame = parseImagePayload(payload.data(), payload.size());
                emit imageFrameReady(frame);
            }
            else if (topicItem.first == "/aft_mapped_to_init") {
                OdomFrame frame = parseOdomPayload(payload.data(), payload.size());
                frame.index = value;
                emit odomFrameReady(frame);
            }
        }
    }
}

GeneralCloudFrame BagWorker::parseLivoxPayload(const uint8_t* payload, size_t length) {
    GeneralCloudFrame frame;
    size_t offset = 0;

    // std_msgs/Header
    offset += 12; // seq(4) + stamp_sec(4) + stamp_nsec(4)
    uint32_t frame_id_len = *(uint32_t*)(payload + offset); offset += 4;
    frame.frame_id = QString::fromUtf8((const char*)(payload + offset), frame_id_len);
    offset += frame_id_len;

    // Livox CustomMsg Header
    frame.timestamp = *(uint64_t*)(payload + offset); offset += 8; // timebase
    uint32_t point_num = *(uint32_t*)(payload + offset); offset += 4; // point_num
    offset += 1; // lidar_id
    offset += 3; // rsvd

    uint32_t array_elements = *(uint32_t*)(payload + offset); offset += 4;

    frame.points.resize(point_num);
    for (int i = 0; i < point_num; i++) {
        const LivoxPoint* src = reinterpret_cast<const LivoxPoint*>(payload + offset + i * sizeof(LivoxPoint));
        GeneralPointI tempPoint;
        
        tempPoint.x = src->x;
        tempPoint.y = src->y;
        tempPoint.z = src->z;
        tempPoint.reflectivity = src->reflectivity;
        frame.points[i].pointI = tempPoint;
        // RGB 颜色
        frame.points[i].r = JET_LUT[tempPoint.reflectivity].r;
        frame.points[i].g = JET_LUT[tempPoint.reflectivity].g;
        frame.points[i].b = JET_LUT[tempPoint.reflectivity].b;
    }

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

OdomFrame BagWorker::parseOdomPayload(const uint8_t* payload, size_t length) {
    OdomFrame odom;
    size_t offset = 0;

    uint32_t seq = *(uint32_t*)(payload + offset); offset += 4;
    uint32_t sec = *(uint32_t*)(payload + offset); offset += 4;
    uint32_t nsec = *(uint32_t*)(payload + offset); offset += 4;
    odom.timestamp = (uint64_t)sec * 1000000000ULL + nsec; // 合成纳秒级时间戳

    uint32_t frame_id_len = *(uint32_t*)(payload + offset); offset += 4;
    odom.frame_id = QString::fromUtf8((const char*)(payload + offset), frame_id_len);
    offset += frame_id_len;

    uint32_t child_frame_id_len = *(uint32_t*)(payload + offset); offset += 4;
    odom.child_frame_id = QString::fromUtf8((const char*)(payload + offset), child_frame_id_len);
    offset += child_frame_id_len;

    odom.pose.x = *(double*)(payload + offset); offset += 8;
    odom.pose.y = *(double*)(payload + offset); offset += 8;
    odom.pose.z = *(double*)(payload + offset); offset += 8;
    
    odom.pose.qx = *(double*)(payload + offset); offset += 8;
    odom.pose.qy = *(double*)(payload + offset); offset += 8;
    odom.pose.qz = *(double*)(payload + offset); offset += 8;
    odom.pose.qw = *(double*)(payload + offset); offset += 8;

    offset += (36 * 8);

    odom.twist.linear_x = *(double*)(payload + offset); offset += 8;
    odom.twist.linear_y = *(double*)(payload + offset); offset += 8;
    odom.twist.linear_z = *(double*)(payload + offset); offset += 8;


    odom.twist.angular_x = *(double*)(payload + offset); offset += 8;
    odom.twist.angular_y = *(double*)(payload + offset); offset += 8;
    odom.twist.angular_z = *(double*)(payload + offset); offset += 8;

    // 跳过 Twist 的协方差矩阵 (36 * 8 字节)
    offset += (36 * 8);

    // 越界检查
    if (offset > length) {
        qWarning() << "Odometry 数据解析越界，可能数据包不完整！";
    }


    return odom;
}