#include "BagWorker.h"
#include "core/rosbag.h"
#include <QDebug>
#include <QImage>
#include <QUuid>
#include <execution>
#include <future>
#include <numeric>

BagWorker::BagWorker(QObject* parent) : QObject(parent), stop_flag(false) {}

void BagWorker::stopProcessing() {
  stop_flag = true;
}

QString BagWorker::generateUUID() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

void BagWorker::processBag(const QString& bag_path) {
  stop_flag = false;

  Rosbag bag(bag_path.toStdString());
  std::vector<std::string> topic_list = bag.getAvailableTopics();
  std::vector<std::string> type_list = bag.getAvailableTypes();

  emit topicListReady(topic_list);
  
  QString bag_uuid = generateUUID();

  for (int i = 0; i < topic_list.size(); i++) {
    std::string topic = topic_list[i];
    std::string msg_type = type_list[i];
    
    emit topicInfoReady(bag_uuid, QString::fromStdString(topic), QString::fromStdString(msg_type));
  }

  const std::string path_str = bag_path.toStdString();
  using TopicPayloads = std::pair<std::string, std::vector<std::vector<uint8_t>>>;
  std::vector<std::future<TopicPayloads>> futures;
  futures.reserve(topic_list.size());

  for (const std::string& topic : topic_list) {
    futures.push_back(std::async(std::launch::async,
      [path_str, topic]() -> TopicPayloads {
        qDebug() << "Available topic in bag:" << QString::fromStdString(topic);
        Rosbag topic_bag(path_str);
        return { topic, topic_bag.getRawPayloads(topic) };
      }));
  }

  int max_size = 0;
  for (auto& fut : futures) {
    auto [topic, payloads] = fut.get();
    max_size = std::max(max_size, static_cast<int>(payloads.size()));
    for (int i = 0; i < payloads.size(); ++i) {
        QString topic_name = QString::fromStdString(topic);
        qint64 timestamp = 0;

        QByteArray payload_data(reinterpret_cast<const char*>(payloads[i].data()), payloads[i].size());

        emit payloadReady(topic_name, i, timestamp, payload_data);
    }
  }

  if (max_size == 0) {
    emit finished();
    return;
  }
  emit messageNumReady(max_size);
  emit finished();
}

void BagWorker::updateProgress(const QString& topic_name, const int percent, const QByteArray& payload_data) {
    const uint8_t* payload = reinterpret_cast<const uint8_t*>(payload_data.constData());
    if (topic_name == "/livox/lidar") {
    GeneralCloudFrame frame = parseLivoxPayload(payload, payload_data.size());
    emit cloudFrameReady(frame);
    } else if (topic_name == "/cloud_registered") {
    GeneralCloudFrame frame = parseSensorPc2Payload(payload, payload_data.size());
    emit cloudFrameReady(frame);
    } else if (topic_name == "/usb_cam/image_raw/compressed") {
    ImageFrame frame = parseImagePayload(payload, payload_data.size());
    emit imageFrameReady(frame);
    } else if (topic_name == "/aft_mapped_to_init") {
    OdomFrame frame = parseOdomPayload(payload, payload_data.size());
    frame.index = percent;
    emit odomFrameReady(frame);
    }
}

GeneralCloudFrame BagWorker::parseLivoxPayload(const uint8_t* payload, size_t length) {
  GeneralCloudFrame frame;
  size_t offset = 0;

  offset += 12;
  uint32_t frame_id_len = *(uint32_t*)(payload + offset);
  offset += 4;
  frame.frame_id = QString::fromUtf8((const char*)(payload + offset), frame_id_len);
  offset += frame_id_len;

  frame.timestamp = *(uint64_t*)(payload + offset);
  offset += 8;
  uint32_t point_num = *(uint32_t*)(payload + offset);
  offset += 4;
  offset += 1;
  offset += 3;

  uint32_t array_elements = *(uint32_t*)(payload + offset);
  offset += 4;

  frame.points.resize(point_num);

  const size_t points_offset = offset;

  std::vector<uint32_t> indices(point_num);
  std::iota(indices.begin(), indices.end(), 0u);
  std::for_each(std::execution::par_unseq,
                indices.begin(), indices.end(),
                [&frame, payload, points_offset](uint32_t i) {
                  const LivoxPoint* src = reinterpret_cast<const LivoxPoint*>(
                      payload + points_offset + i * sizeof(LivoxPoint));
                  GeneralPointIRGB& pt = frame.points[i];
                  pt.point_i.x = src->x;
                  pt.point_i.y = src->y;
                  pt.point_i.z = src->z;
                  pt.point_i.reflectivity = src->reflectivity;
                  pt.r = kJetLut[src->reflectivity].r;
                  pt.g = kJetLut[src->reflectivity].g;
                  pt.b = kJetLut[src->reflectivity].b;
                });

  return frame;
}

GeneralCloudFrame BagWorker::parseSensorPc2Payload(const uint8_t* payload, size_t length) {
  GeneralCloudFrame frame;
  size_t offset = 0;

  offset += 12;
  uint32_t frame_id_len = *(uint32_t*)(payload + offset);
  offset += 4;
  frame.frame_id = QString::fromUtf8((const char*)(payload + offset), frame_id_len);
  offset += frame_id_len;

  uint32_t height = *(uint32_t*)(payload + offset);
  offset += 4;
  uint32_t width = *(uint32_t*)(payload + offset);
  offset += 4;
  uint32_t point_num = height * width;

  uint32_t fields_size = *(uint32_t*)(payload + offset);
  offset += 4;
  struct FieldInfo {
    std::string name;
    uint32_t offset;
    uint8_t datatype;
    uint32_t count;
  };
  std::vector<FieldInfo> fields;
  for (uint32_t i = 0; i < fields_size; ++i) {
    uint32_t name_len = *(uint32_t*)(payload + offset);
    offset += 4;
    std::string name((const char*)(payload + offset), name_len);
    offset += name_len;
    uint32_t field_offset = *(uint32_t*)(payload + offset);
    offset += 4;
    uint8_t datatype = *(uint8_t*)(payload + offset);
    offset += 1;
    uint32_t count = *(uint32_t*)(payload + offset);
    offset += 4;
    fields.push_back({ name, field_offset, datatype, count });
  }

  bool is_bigendian = *(uint8_t*)(payload + offset);
  offset += 1;
  uint32_t point_step = *(uint32_t*)(payload + offset);
  offset += 4;
  uint32_t row_step = *(uint32_t*)(payload + offset);
  offset += 4;

  uint32_t data_len = *(uint32_t*)(payload + offset);
  offset += 4;
  const uint8_t* data_ptr = payload + offset;
  offset += data_len;

  bool is_dense = *(uint8_t*)(payload + offset);
  offset += 1;

  int x_idx = -1, y_idx = -1, z_idx = -1, rgb_idx = -1, reflectivity_idx = -1;
  for (size_t i = 0; i < fields.size(); ++i) {
    if (fields[i].name == "x") x_idx = i;
    if (fields[i].name == "y") y_idx = i;
    if (fields[i].name == "z") z_idx = i;
    if (fields[i].name == "rgb" || fields[i].name == "rgba") rgb_idx = i;
    if (fields[i].name == "intensity" || fields[i].name == "reflectivity") reflectivity_idx = i;
  }

  if (data_len < point_num * point_step) {
    qWarning() << "PointCloud2 data_len is abnormal! Expected at least:" << point_num * point_step << "Actual:" << data_len;
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
      if (dtype == 7) {
        pt.reflectivity = (uint8_t)(*(float*)(data_ptr + base + fields[reflectivity_idx].offset));
      } else if (dtype == 2) {
        pt.reflectivity = *(uint8_t*)(data_ptr + base + fields[reflectivity_idx].offset);
      } else {
        pt.reflectivity = 0;
      }
    }
    frame.points[i].point_i = pt;

    if (rgb_idx >= 0) {
      uint32_t rgb = *(uint32_t*)(data_ptr + base + fields[rgb_idx].offset);
      frame.points[i].r = (rgb >> 16) & 0xFF;
      frame.points[i].g = (rgb >> 8) & 0xFF;
      frame.points[i].b = rgb & 0xFF;
    } else {
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

  uint32_t seq = *(uint32_t*)(payload + offset);
  offset += 4;
  uint32_t sec = *(uint32_t*)(payload + offset);
  offset += 4;
  uint32_t nsec = *(uint32_t*)(payload + offset);
  offset += 4;
  frame.timestamp = (uint64_t)sec * 1000000000ULL + nsec;

  uint32_t frame_id_len = *(uint32_t*)(payload + offset);
  offset += 4;
  offset += frame_id_len;

  uint32_t format_len = *(uint32_t*)(payload + offset);
  offset += 4;
  QString format_str = QString::fromUtf8((const char*)(payload + offset), format_len);
  offset += format_len;

  uint32_t data_len = *(uint32_t*)(payload + offset);
  offset += 4;

  if (offset + data_len > length) {
    qWarning() << "CompressedImage data packet is incomplete, out of bounds!";
    return frame;
  }

  bool success = frame.image.loadFromData(payload + offset, data_len);
  if (!success) {
    qWarning() << "Image decoding failed! ROS format field:" << format_str;
  }

  return frame;
}

OdomFrame BagWorker::parseOdomPayload(const uint8_t* payload, size_t length) {
  OdomFrame odom;
  size_t offset = 0;

  uint32_t seq = *(uint32_t*)(payload + offset);
  offset += 4;
  uint32_t sec = *(uint32_t*)(payload + offset);
  offset += 4;
  uint32_t nsec = *(uint32_t*)(payload + offset);
  offset += 4;
  odom.timestamp = (uint64_t)sec * 1000000000ULL + nsec;

  uint32_t frame_id_len = *(uint32_t*)(payload + offset);
  offset += 4;
  odom.frame_id = QString::fromUtf8((const char*)(payload + offset), frame_id_len);
  offset += frame_id_len;

  uint32_t child_frame_id_len = *(uint32_t*)(payload + offset);
  offset += 4;
  odom.child_frame_id = QString::fromUtf8((const char*)(payload + offset), child_frame_id_len);
  offset += child_frame_id_len;

  odom.pose.x = *(double*)(payload + offset);
  offset += 8;
  odom.pose.y = *(double*)(payload + offset);
  offset += 8;
  odom.pose.z = *(double*)(payload + offset);
  offset += 8;
  
  odom.pose.qx = *(double*)(payload + offset);
  offset += 8;
  odom.pose.qy = *(double*)(payload + offset);
  offset += 8;
  odom.pose.qz = *(double*)(payload + offset);
  offset += 8;
  odom.pose.qw = *(double*)(payload + offset);
  offset += 8;

  offset += (36 * 8);

  odom.twist.linear_x = *(double*)(payload + offset);
  offset += 8;
  odom.twist.linear_y = *(double*)(payload + offset);
  offset += 8;
  odom.twist.linear_z = *(double*)(payload + offset);
  offset += 8;

  odom.twist.angular_x = *(double*)(payload + offset);
  offset += 8;
  odom.twist.angular_y = *(double*)(payload + offset);
  offset += 8;
  odom.twist.angular_z = *(double*)(payload + offset);
  offset += 8;

  offset += (36 * 8);

  if (offset > length) {
    qWarning() << "Odometry data packet out of bounds, possible data corruption";
  }

  return odom;
}
