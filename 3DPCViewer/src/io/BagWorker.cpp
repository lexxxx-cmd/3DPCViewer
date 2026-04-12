#include "BagWorker.h"
#include "core/rosbag.h"
#include "db/CDRConverter.h"
#include <QDebug>
#include <QImage>
#include <execution>
#include <numeric>

BagWorker::BagWorker(QObject* parent) : QObject(parent), m_stopFlag(false) {}

namespace {
constexpr int kCdrEncapsulationHeaderSize = 4;
}

void BagWorker::stopProcessing() {
    m_stopFlag = true;
}

void BagWorker::processBag(const QString& bagPath, int bagIndex) {
    m_stopFlag = false;
    m_currentBagIndex = bagIndex;

    // Open bag once to retrieve the topic list (metadata only, very fast).
    Rosbag bag(bagPath.toStdString());
    std::vector<std::string> topicList = bag.getAvailableTopics();

    std::vector<std::string> bagTopicList;
    bagTopicList.reserve(topicList.size());
    for (const std::string& topic : topicList) {
        bagTopicList.emplace_back("/bag" + std::to_string(bagIndex) + topic);
    }
    emit topicListReady(bagTopicList);

    // Launch one async task per topic so their payload reads run in parallel.
    // Each task opens its own Rosbag (and therefore its own file descriptor)
    // to avoid contention on the shared ifstream.
    const std::string pathStr = bagPath.toStdString();

    struct TopicData {
        std::string                        topic;
        std::vector<std::vector<uint8_t>>  payloads;
        std::vector<int64_t>               timestamps;
        std::string                        typeName;
    };

    std::vector<std::future<TopicData>> futures;
    futures.reserve(topicList.size());

    for (const std::string& topic : topicList) {
        futures.push_back(std::async(std::launch::async,
            [pathStr, topic]() -> TopicData {
                qDebug() << "Bag 中可用 Topic:" << QString::fromStdString(topic);
                Rosbag topicBag(pathStr);
                TopicData td;
                td.topic      = topic;
                td.payloads   = topicBag.getRawPayloads(topic);
                td.timestamps = topicBag.getMessageTimestamps(topic);
                td.typeName   = topicBag.getTopicType(topic);
                return td;
            }));
    }

    // Collect results and build the DB write pipeline.
    int max_size = 0;
    for (auto& fut : futures) {
        auto td = fut.get();
        max_size = std::max(max_size, static_cast<int>(td.payloads.size()));

        // Emit each payload for DB storage (passing msgIndex for database)
        const std::string& topic = td.topic;
        const std::string& typeName = td.typeName;

        for (size_t i = 0; i < td.payloads.size(); ++i) {
            if (m_stopFlag) break;

            const auto& payload = td.payloads[i];
            int msgIndex = static_cast<int>(i);

            RawBagMessage msg;
            msg.bagIndex    = bagIndex;
            msg.topicName   = QString::fromStdString(topic);
            msg.typeName    = QString::fromStdString(typeName);
            msg.timestampNs = (i < td.timestamps.size()) ? td.timestamps[i] : 0LL;

            CDRConverter::packRawBytesToCDR(
                reinterpret_cast<const char*>(payload.data()),
                static_cast<int>(payload.size()),
                msg.rawData);

            emit parsedMessageReady(msg, msgIndex);
        }
    }

    // Verify that at least one message was read.
    if (max_size == 0) {
        emit finished();
        return;
    }
    emit messageNumReady(max_size);

    emit finished();
}

// ---------------------------------------------------------------------------
// parseMessages - Parse raw messages from DB and emit frame signals
// ---------------------------------------------------------------------------
void BagWorker::parseMessages(const std::vector<RawBagMessage>& messages, int msgIndex) {
    for (const RawBagMessage& msg : messages) {
        if (m_stopFlag) break;

        const QByteArray& cdrData = msg.rawData;
        if (cdrData.size() <= kCdrEncapsulationHeaderSize) {
            continue;
        }

        // Strip the 4-byte DDS encapsulation header
        const auto* payloadBegin =
            reinterpret_cast<const uint8_t*>(cdrData.constData() + kCdrEncapsulationHeaderSize);
        const auto* payloadEnd =
            reinterpret_cast<const uint8_t*>(cdrData.constData() + cdrData.size());
        size_t payloadSize = static_cast<size_t>(cdrData.size() - kCdrEncapsulationHeaderSize);

        const std::string topic = msg.topicName.toStdString();

        if (topic == "/livox/lidar") {
            GeneralCloudFrame frame = parseLivoxPayload(payloadBegin, payloadSize);
            emit cloudFrameReady(frame);
        }
        else if (topic == "/cloud_registered") {
            GeneralCloudFrame frame = parseSensorPC2Payload(payloadBegin, payloadSize);
            emit cloudFrameReady(frame);
        }
        else if (topic == "/usb_cam/image_raw/compressed") {
            ImageFrame frame = parseImagePayload(payloadBegin, payloadSize);
            emit imageFrameReady(frame);
        }
        else if (topic == "/aft_mapped_to_init") {
            OdomFrame frame = parseOdomPayload(payloadBegin, payloadSize);
            frame.index = msgIndex;
            emit odomFrameReady(frame);
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

    // Capture offset by value so the parallel lambda reads a stable snapshot.
    const size_t pointsOffset = offset;

    // Use an index range and parallel execution: each point conversion is independent.
    std::vector<uint32_t> indices(point_num);
    std::iota(indices.begin(), indices.end(), 0u);
    std::for_each(std::execution::par_unseq,
                  indices.begin(), indices.end(),
                  [&frame, payload, pointsOffset](uint32_t i) {
                      const LivoxPoint* src = reinterpret_cast<const LivoxPoint*>(
                          payload + pointsOffset + i * sizeof(LivoxPoint));
                      GeneralPointIRGB& pt = frame.points[i];
                      pt.pointI.x            = src->x;
                      pt.pointI.y            = src->y;
                      pt.pointI.z            = src->z;
                      pt.pointI.reflectivity = src->reflectivity;
                      pt.r = JET_LUT[src->reflectivity].r;
                      pt.g = JET_LUT[src->reflectivity].g;
                      pt.b = JET_LUT[src->reflectivity].b;
                  });

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
    odom.timestamp = (uint64_t)sec * 1000000000ULL + nsec;

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

    // 越界检查
    if (offset > length) {
        qWarning() << "Odometry 数据包越界！";
    }


    return odom;
}
