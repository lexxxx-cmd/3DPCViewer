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
            else if (topicItem.first == "/cloud_registered") {
                GeneralCloudFrame frame = parseSensorPC2Payload(payload.data(), payload.size());
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

GeneralCloudFrame BagWorker::parseSensorPC2Payload(const uint8_t* payload, size_t length) {
    GeneralCloudFrame frame;
    size_t offset = 0;

    // 解析Header
    offset += 12; // seq(4) + stamp_sec(4) + stamp_nsec(4)
    uint32_t frame_id_len = *(uint32_t*)(payload + offset); offset += 4;
    frame.frame_id = QString::fromUtf8((const char*)(payload + offset), frame_id_len);
    offset += frame_id_len;

    // 解析height和width
    uint32_t height = *(uint32_t*)(payload + offset); offset += 4;
    uint32_t width = *(uint32_t*)(payload + offset); offset += 4;
    uint32_t point_num = height * width;

    // fields数组
    uint32_t fields_size = *(uint32_t*)(payload + offset); offset += 4;
    struct FieldInfo {
        std::string name;
        uint32_t offset;
        uint8_t datatype;
        uint32_t count;
    };
    std::vector<FieldInfo> fields;
    for (uint32_t i = 0; i < fields_size; ++i) {
        uint32_t name_len = *(uint32_t*)(payload + offset); offset += 4;
        std::string name((const char*)(payload + offset), name_len);
        offset += name_len;
        uint32_t field_offset = *(uint32_t*)(payload + offset); offset += 4;
        uint8_t datatype = *(uint8_t*)(payload + offset); offset += 1;
        uint32_t count = *(uint32_t*)(payload + offset); offset += 4;
        fields.push_back({ name, field_offset, datatype, count });
    }

    bool is_bigendian = *(uint8_t*)(payload + offset); offset += 1;
    uint32_t point_step = *(uint32_t*)(payload + offset); offset += 4;
    uint32_t row_step = *(uint32_t*)(payload + offset); offset += 4;

    uint32_t data_len = *(uint32_t*)(payload + offset); offset += 4;
    const uint8_t* data_ptr = payload + offset;
    offset += data_len;

    bool is_dense = *(uint8_t*)(payload + offset); offset += 1;

    int x_idx = -1, y_idx = -1, z_idx = -1, rgb_idx = -1, reflectivity_idx = -1;
    for (size_t i = 0; i < fields.size(); ++i) {
        if (fields[i].name == "x") x_idx = i;
        if (fields[i].name == "y") y_idx = i;
        if (fields[i].name == "z") z_idx = i;
        if (fields[i].name == "rgb" || fields[i].name == "rgba") rgb_idx = i;
        if (fields[i].name == "intensity" || fields[i].name == "reflectivity") reflectivity_idx = i;
    }

    // 防止 data_len 不足导致的内存越界崩溃
    if (data_len < point_num * point_step) {
        qWarning() << "PointCloud2 data_len 异常! 预期至少:" << point_num * point_step << "实际:" << data_len;
        point_num = data_len / point_step;
    }

    frame.points.resize(point_num);
    for (uint32_t i = 0; i < point_num; ++i) {
        size_t base = i * point_step;
        GeneralPointI pt;

        if (x_idx >= 0) pt.x = *(float*)(data_ptr + base + fields[x_idx].offset);
        if (y_idx >= 0) pt.y = *(float*)(data_ptr + base + fields[y_idx].offset);
        if (z_idx >= 0) pt.z = *(float*)(data_ptr + base + fields[z_idx].offset);

        if (reflectivity_idx >= 0) {
            uint8_t dtype = fields[reflectivity_idx].datatype;
            if (dtype == 7) { // FLOAT32
                pt.reflectivity = (uint8_t)(*(float*)(data_ptr + base + fields[reflectivity_idx].offset));
            }
            else if (dtype == 2) { // UINT8
                pt.reflectivity = *(uint8_t*)(data_ptr + base + fields[reflectivity_idx].offset);
            }
            else {
                pt.reflectivity = 0; // 默认值
            }
        }
        frame.points[i].pointI = pt;

        // 解析RGB
        if (rgb_idx >= 0) {
            uint32_t rgb = *(uint32_t*)(data_ptr + base + fields[rgb_idx].offset);
            frame.points[i].r = (rgb >> 16) & 0xFF;
            frame.points[i].g = (rgb >> 8) & 0xFF;
            frame.points[i].b = rgb & 0xFF;
        }
        else {
            frame.points[i].r = pt.reflectivity;
            frame.points[i].g = pt.reflectivity;
            frame.points[i].b = pt.reflectivity;
        }
    }

    return frame;
}


ImageFrame BagWorker::parseImagePayload(const uint8_t* payload, size_t length) {
    ImageFrame frame;
    size_t offset = 0;

    // 1. 解析 Header
    uint32_t seq = *(uint32_t*)(payload + offset); offset += 4;
    uint32_t sec = *(uint32_t*)(payload + offset); offset += 4;
    uint32_t nsec = *(uint32_t*)(payload + offset); offset += 4;
    frame.timestamp = (uint64_t)sec * 1000000000ULL + nsec;

    uint32_t frame_id_len = *(uint32_t*)(payload + offset); offset += 4;
    offset += frame_id_len;

    uint32_t format_len = *(uint32_t*)(payload + offset); offset += 4;
    QString format_str = QString::fromUtf8((const char*)(payload + offset), format_len);
    offset += format_len;

    uint32_t data_len = *(uint32_t*)(payload + offset); offset += 4;

    // 越界保护
    if (offset + data_len > length) {
        qWarning() << "CompressedImage 数据包不完整，发生越界！";
        return frame;
    }

    bool success = frame.image.loadFromData(payload + offset, data_len);
    if (!success) {
        qWarning() << "图像解码失败！ROS format 字段为:" << format_str;
    }

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