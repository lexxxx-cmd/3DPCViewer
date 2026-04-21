#include "BagWorker.h"
#include "core/rosbag.h"
#include <QDebug>
#include <QImage>
#include <QUuid>
#include <QFileInfo>
#include <execution>
#include <future>
#include <numeric>
#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <cstdint>
#include <Eigen/Core>
#include <Eigen/Geometry>

template <typename T>
void ReadBinaryLittleEndian(std::istream* stream, T* data) {
    stream->read(reinterpret_cast<char*>(data), sizeof(T));
}

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
  std::unordered_map<std::string, std::string> topic_to_type;

  QString bag_uuid = generateUUID();

  for (int i = 0; i < topic_list.size(); i++) {
    std::string topic = topic_list[i];
    std::string msg_type = type_list[i];
    topic_to_type[topic] = msg_type;

    emit topicInfoReady(bag_uuid, QString::fromStdString(topic), QString::fromStdString(msg_type));
  }

  const std::string path_str = bag_path.toStdString();
  using TopicPayloads = std::pair<std::string, std::vector<std::pair<int64_t, std::vector<uint8_t>>>>;
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
    std::string msg_type = topic_to_type[topic];

    for (int i = 0; i < payloads.size(); ++i) {
        QString topic_name = QString::fromStdString(topic);
        qint64 timestamp = payloads[i].first; // Accurately parsed Bag receipt time

        QByteArray payload_data(reinterpret_cast<const char*>(payloads[i].second.data()), payloads[i].second.size());

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



void BagWorker::processBin(const QString& bin_path) {
    std::ifstream file(bin_path.toStdString(), std::ios::binary);
    if (!file.is_open()) {
        // 修复中文乱码
        std::cerr << "无法打开文件: " << bin_path.toStdString() << std::endl;
        return;
    }

    QFileInfo fileInfo(bin_path);
    QString fileName = fileInfo.fileName();

    if (fileName.startsWith("images")) {
        uint64_t num_images;
        ReadBinaryLittleEndian(&file, &num_images);
        std::cout << "images.bin: " << num_images << std::endl;

        // 定义统一的坐标系转换矩阵: 绕X轴旋转-90度 (COLMAP -> OSG)
        Eigen::Matrix3d Rx_m90;
        Rx_m90 << 1, 0, 0,
            0, 0, 1,
            0, -1, 0;

        for (size_t i = 0; i < num_images; ++i) {
            uint32_t image_id;
            ReadBinaryLittleEndian(&file, &image_id);

            double qw, qx, qy, qz, tx, ty, tz;
            ReadBinaryLittleEndian(&file, &qw);
            ReadBinaryLittleEndian(&file, &qx);
            ReadBinaryLittleEndian(&file, &qy);
            ReadBinaryLittleEndian(&file, &qz);
            ReadBinaryLittleEndian(&file, &tx);
            ReadBinaryLittleEndian(&file, &ty);
            ReadBinaryLittleEndian(&file, &tz);

            uint32_t camera_id;
            ReadBinaryLittleEndian(&file, &camera_id);

            // 读取字符串 (小优化：虽然还是单字符循环，但预先分配避免频繁扩容)
            std::string name;
            name.reserve(64);
            char name_char;
            while (file.read(&name_char, 1) && name_char != '\0') {
                name += name_char;
            }

            // --- 核心变换逻辑 ---
            Eigen::Quaterniond q(qw, qx, qy, qz);
            Eigen::Matrix3d R = q.toRotationMatrix();
            Eigen::Vector3d t(tx, ty, tz);

            // 1. 获取 COLMAP 坐标系下的 C2W 位置
            Eigen::Vector3d camera_center = -R.transpose() * t;

            // 2. 将位置转换到 OSG 坐标系
            Eigen::Vector3d camera_center_osg = Rx_m90 * camera_center;

            // 3. 将姿态转换到 OSG 坐标系
            // 原始 R.transpose() 是 COLMAP 坐标系下的 C2W 旋转
            // 左乘 Rx_m90 代表在新的世界坐标系下描述该相机的朝向
            Eigen::Matrix3d R_osg = Rx_m90 * R.transpose();
            Eigen::Quaterniond q_osg(R_osg);

            // 跳过 2D 点数据
            uint64_t num_points2D;
            ReadBinaryLittleEndian(&file, &num_points2D);
            file.seekg(num_points2D * 24, std::ios::cur);

            // 封装数据 (统一使用转换后的 osg 变量)
            OdomFrame Frame;
            Frame.timestamp = i; // 或者 image_id
            Frame.index = i;
            Frame.pose.qw = q_osg.w();
            Frame.pose.qx = q_osg.x();
            Frame.pose.qy = q_osg.y();
            Frame.pose.qz = q_osg.z();
            Frame.pose.x = camera_center_osg.x();
            Frame.pose.y = camera_center_osg.y();
            Frame.pose.z = camera_center_osg.z();
            emit odomFrameReady(Frame);
        }
    }
    else if (fileName.startsWith("points3D")) {
        uint64_t num_points3D;
        ReadBinaryLittleEndian(&file, &num_points3D);
        std::cout << "正在处理 points3D.bin, 总点数: " << num_points3D << std::endl;

        GeneralCloudFrame Frame;
        Frame.points.resize(num_points3D);
        Frame.frame_id = "1";
        Frame.timestamp = 1;

        for (size_t i = 0; i < num_points3D; ++i) {
            uint64_t point3D_id;
            ReadBinaryLittleEndian(&file, &point3D_id);

            double x, y, z;
            ReadBinaryLittleEndian(&file, &x);
            ReadBinaryLittleEndian(&file, &y);
            ReadBinaryLittleEndian(&file, &z);

            uint8_t r, g, b;
            ReadBinaryLittleEndian(&file, &r);
            ReadBinaryLittleEndian(&file, &g);
            ReadBinaryLittleEndian(&file, &b);

            double error;
            ReadBinaryLittleEndian(&file, &error);

            uint64_t track_len;
            ReadBinaryLittleEndian(&file, &track_len);
            file.seekg(track_len * 8, std::ios::cur);

            // COLMAP坐标系 -> OSG坐标系: 绕X轴旋转-90度
            // 这里手工映射是对的，因为运算非常简单没必要用 Eigen，性能更好
            Frame.points[i].point_i.x = static_cast<float>(x);
            Frame.points[i].point_i.y = static_cast<float>(z);
            Frame.points[i].point_i.z = -static_cast<float>(y);
            Frame.points[i].r = r;
            Frame.points[i].g = g;
            Frame.points[i].b = b;
        }

        emit cloudFrameReady(Frame);
    }

    file.close();
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
      const uint8_t* color_ptr = data_ptr + base + fields[rgb_idx].offset;
      frame.points[i].b = color_ptr[0];
      frame.points[i].g = color_ptr[1];
      frame.points[i].r = color_ptr[2];
    } else {
      frame.points[i].r = kJetLut[pt.reflectivity].r;
      frame.points[i].g = kJetLut[pt.reflectivity].g;
      frame.points[i].b = kJetLut[pt.reflectivity].b;
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
