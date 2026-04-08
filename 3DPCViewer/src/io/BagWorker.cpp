#include "BagWorker.h"
#include "core/rosbag.h"
#include <QDebug>
#include <QImage>
#include <execution>
#include <future>
#include <numeric>

BagWorker::BagWorker(QObject* parent) : QObject(parent), m_stopFlag(false) {}

void BagWorker::stopProcessing() {
    m_stopFlag = true;
}

void BagWorker::processBag(const QString& bagPath) {
    m_stopFlag = false;

    // Open bag once to retrieve the topic list (metadata only, very fast).
    Rosbag bag(bagPath.toStdString());
    std::vector<std::string> topicList = bag.getAvailableTopics();

    emit topicListReady(topicList);

    // Guard: if the cache already contains data from this bag, clear it first.
    for (const std::string& topic : topicList) {
        if (m_bagCache.find(topic) != m_bagCache.end()) {
            // TODO
            m_bagCache.clear();
            return;
        }
    }

    // Launch one async task per topic so their payload reads run in parallel.
    // Each task opens its own Rosbag (and therefore its own file descriptor)
    // to avoid contention on the shared ifstream.
    const std::string pathStr = bagPath.toStdString();
    using TopicPayloads = std::pair<std::string, std::vector<std::vector<uint8_t>>>;
    std::vector<std::future<TopicPayloads>> futures;
    futures.reserve(topicList.size());

    for (const std::string& topic : topicList) {
        futures.push_back(std::async(std::launch::async,
            [pathStr, topic]() -> TopicPayloads {
                qDebug() << "Bag 中可用 Topic:" << QString::fromStdString(topic);
                Rosbag topicBag(pathStr);
                return { topic, topicBag.getRawPayloads(topic) };
            }));
    }

    // Collect results and build the cache.
    int max_size = 0;
    for (auto& fut : futures) {
        auto [topic, payloads] = fut.get();
        max_size = std::max(max_size, static_cast<int>(payloads.size()));
        m_bagCache[topic] = std::move(payloads);
    }

    // Verify that at least one message was read.
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

ImageFrame BagWorker::parseImagePayload(const uint8_t* payload, size_t length) {
    ImageFrame frame;
    size_t offset = 0;

    // 1. ��� Header
    offset += 12;
    uint32_t frame_id_len = *(uint32_t*)(payload + offset); offset += 4;
    offset += frame_id_len;

    // 2. ��� format �ַ���
    uint32_t format_len = *(uint32_t*)(payload + offset); offset += 4;
    offset += format_len;

    // 3. ��ȡͼ�����ݳ���
    uint32_t data_len = *(uint32_t*)(payload + offset); offset += 4;
    if (offset + data_len > length) {
        return frame; // Խ�籣��
    }
    // 4. Qt ԭ����ѹ JPEG/PNG
    frame.image.loadFromData(payload + offset, data_len);

    return frame;
}

OdomFrame BagWorker::parseOdomPayload(const uint8_t* payload, size_t length) {
    OdomFrame odom;
    size_t offset = 0;

    uint32_t seq = *(uint32_t*)(payload + offset); offset += 4;
    uint32_t sec = *(uint32_t*)(payload + offset); offset += 4;
    uint32_t nsec = *(uint32_t*)(payload + offset); offset += 4;
    odom.timestamp = (uint64_t)sec * 1000000000ULL + nsec; // �ϳ����뼶ʱ���

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

    // ���� Twist ��Э������� (36 * 8 �ֽ�)
    offset += (36 * 8);

    // Խ����
    if (offset > length) {
        qWarning() << "Odometry ���ݽ���Խ�磬�������ݰ���������";
    }


    return odom;
}