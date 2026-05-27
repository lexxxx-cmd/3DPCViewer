#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <string>
#include <cstring>
#include <zmq.hpp>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/common/transforms.h>
#include <pcl/io/pcd_io.h>
#include <filesystem>
#include <deque>
#include <opencv2/opencv.hpp>

struct ZmqMessage {
    std::string header;
    std::vector<uint8_t> payload;
};

struct OdomPose {
    double x, y, z;
    double qx, qy, qz, qw;
};

struct OdomFrame {
    uint64_t timestamp;
    std::string frame_id;
    std::string child_frame_id;
    OdomPose pose;
};

struct PcdFrame {
    uint64_t timestamp;
    pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud;
};

struct ImageFrame {
    uint64_t timestamp;
    std::vector<uint8_t> data;  // raw compressed image bytes (no decode)
    std::string format;         // e.g. "jpeg"
};

using PointT = pcl::PointXYZRGB;
using PointCloud = pcl::PointCloud<PointT>;

class MessageQueue {
public:
    void push(const ZmqMessage& msg) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(msg);
        cond_.notify_one();
    }

    bool pop(ZmqMessage& msg) {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [this]() { return !queue_.empty(); });
        msg = queue_.front();
        queue_.pop();
        return true;
    }

private:
    std::queue<ZmqMessage> queue_;
    std::mutex mutex_;
    std::condition_variable cond_;
};

OdomFrame parseOdomPayload(const uint8_t* payload, size_t length) {
    OdomFrame odom;
    size_t offset = 0;

    uint32_t seq = *(uint32_t*)(payload + offset); offset += 4;
    uint32_t sec = *(uint32_t*)(payload + offset); offset += 4;
    uint32_t nsec = *(uint32_t*)(payload + offset); offset += 4;
    odom.timestamp = (uint64_t)sec * 1000000000ULL + nsec;

    uint32_t frame_id_len = *(uint32_t*)(payload + offset); offset += 4;
    odom.frame_id = std::string((const char*)(payload + offset), frame_id_len); offset += frame_id_len;

    uint32_t child_frame_id_len = *(uint32_t*)(payload + offset); offset += 4;
    odom.child_frame_id = std::string((const char*)(payload + offset), child_frame_id_len); offset += child_frame_id_len;

    odom.pose.x = *(double*)(payload + offset); offset += 8;
    odom.pose.y = *(double*)(payload + offset); offset += 8;
    odom.pose.z = *(double*)(payload + offset); offset += 8;

    odom.pose.qx = *(double*)(payload + offset); offset += 8;
    odom.pose.qy = *(double*)(payload + offset); offset += 8;
    odom.pose.qz = *(double*)(payload + offset); offset += 8;
    odom.pose.qw = *(double*)(payload + offset); offset += 8;

    return odom;
}

PcdFrame parseSensorPc2Payload(const uint8_t* payload, size_t length) {
    PcdFrame pcd_frame;
    PointCloud::Ptr cloud(new PointCloud());
    size_t offset = 0;

    uint32_t seq = *(uint32_t*)(payload + offset); offset += 4;
    uint32_t sec = *(uint32_t*)(payload + offset); offset += 4;
    uint32_t nsec = *(uint32_t*)(payload + offset); offset += 4;
    pcd_frame.timestamp = (uint64_t)sec * 1000000000ULL + nsec;

    uint32_t frame_id_len = *(uint32_t*)(payload + offset); offset += 4;
    std::string frame_id = std::string((const char*)(payload + offset), frame_id_len); offset += frame_id_len;

    uint32_t height = *(uint32_t*)(payload + offset); offset += 4;
    uint32_t width = *(uint32_t*)(payload + offset); offset += 4;
    uint32_t point_num = height * width;

    uint32_t fields_size = *(uint32_t*)(payload + offset); offset += 4;
    struct FieldInfo {
        std::string name; uint32_t offset; uint8_t datatype; uint32_t count;
    };
    std::vector<FieldInfo> fields;
    for (uint32_t i = 0; i < fields_size; ++i) {
        uint32_t name_len = *(uint32_t*)(payload + offset); offset += 4;
        std::string name((const char*)(payload + offset), name_len); offset += name_len;
        uint32_t field_offset = *(uint32_t*)(payload + offset); offset += 4;
        uint8_t datatype = *(uint8_t*)(payload + offset); offset += 1;
        uint32_t count = *(uint32_t*)(payload + offset); offset += 4;
        fields.push_back({ name, field_offset, datatype, count });
    }

    bool is_bigendian = *(uint8_t*)(payload + offset); offset += 1;
    uint32_t point_step = *(uint32_t*)(payload + offset); offset += 4;
    uint32_t row_step = *(uint32_t*)(payload + offset); offset += 4;

    uint32_t data_len = *(uint32_t*)(payload + offset); offset += 4;
    const uint8_t* data_ptr = payload + offset; offset += data_len;

    bool is_dense = *(uint8_t*)(payload + offset); offset += 1;

    int x_idx = -1, y_idx = -1, z_idx = -1, rgb_idx = -1;
    for (size_t i = 0; i < fields.size(); ++i) {
        if (fields[i].name == "x") x_idx = i;
        if (fields[i].name == "y") y_idx = i;
        if (fields[i].name == "z") z_idx = i;
        if (fields[i].name == "rgb" || fields[i].name == "rgba") rgb_idx = i;
    }

    if (data_len < point_num * point_step) point_num = data_len / point_step;

    cloud->points.resize(point_num);
    for (uint32_t i = 0; i < point_num; ++i) {
        size_t base = i * point_step;
        if (x_idx >= 0) cloud->points[i].x = *(float*)(data_ptr + base + fields[x_idx].offset);
        if (y_idx >= 0) cloud->points[i].y = *(float*)(data_ptr + base + fields[y_idx].offset);
        if (z_idx >= 0) cloud->points[i].z = *(float*)(data_ptr + base + fields[z_idx].offset);

        if (rgb_idx >= 0) {
            const uint8_t* color_ptr = data_ptr + base + fields[rgb_idx].offset;
            cloud->points[i].b = color_ptr[0]; cloud->points[i].g = color_ptr[1]; cloud->points[i].r = color_ptr[2];
        } else {
            cloud->points[i].r = 255; cloud->points[i].g = 255; cloud->points[i].b = 255;
        }
    }

    cloud->width = point_num; cloud->height = 1; cloud->is_dense = false;
    pcd_frame.cloud = cloud;
    return pcd_frame;
}

ImageFrame parseImagePayloadLight(const uint8_t* payload, size_t length) {
    ImageFrame img;
    img.timestamp = 0;
    size_t offset = 0;

    if (length < 12) return img;

    uint32_t seq = *reinterpret_cast<const uint32_t*>(payload + offset); offset += 4;
    uint32_t sec = *reinterpret_cast<const uint32_t*>(payload + offset); offset += 4;
    uint32_t nsec = *reinterpret_cast<const uint32_t*>(payload + offset); offset += 4;
    img.timestamp = (uint64_t)sec * 1000000000ULL + nsec;

    uint32_t frame_id_len = *reinterpret_cast<const uint32_t*>(payload + offset); offset += 4;
    offset += frame_id_len;

    uint32_t format_len = *reinterpret_cast<const uint32_t*>(payload + offset); offset += 4;
    img.format = std::string(reinterpret_cast<const char*>(payload + offset), format_len); offset += format_len;

    uint32_t data_len = *reinterpret_cast<const uint32_t*>(payload + offset); offset += 4;
    if (offset + data_len > length) { img.timestamp = 0; return img; }

    img.data.assign(payload + offset, payload + offset + data_len);
    return img;
}

MessageQueue g_queue;

void receiver_thread_func(int port) {
    std::cout << "[PosePcdExporter] Starting ZMQ receiver on port: " << port << std::endl;
    zmq::context_t context(1);
    zmq::socket_t socket(context, zmq::socket_type::pull);
    socket.connect("tcp://127.0.0.1:" + std::to_string(port));

    while (true) {
        zmq::message_t msg_head;
        auto res = socket.recv(msg_head, zmq::recv_flags::none);
        if (!res) continue;

        std::string head_str(static_cast<char*>(msg_head.data()), msg_head.size());

        if (head_str == "[EOF]") {
            std::cout << "[PosePcdExporter] Received EOF. Shutting down receiver." << std::endl;
            ZmqMessage zmq_msg;
            zmq_msg.header = "[EOF]";
            g_queue.push(zmq_msg);
            break;
        }

        int more = socket.get(zmq::sockopt::rcvmore);
        if (more) {
            zmq::message_t msg_body;
            socket.recv(msg_body, zmq::recv_flags::none);
            ZmqMessage zmq_msg;
            zmq_msg.header = head_str;
            zmq_msg.payload.assign(static_cast<uint8_t*>(msg_body.data()), 
                                   static_cast<uint8_t*>(msg_body.data()) + msg_body.size());
            g_queue.push(zmq_msg);
        }
    }
}

int main(int argc, char** argv) {
    if (argc >= 3 && std::string(argv[1]) == "--zmq-port") {
        int port = std::stoi(argv[2]);
        std::thread receiver_thread(receiver_thread_func, port);

        std::deque<OdomFrame> odom_buffer;
        std::deque<PcdFrame> pcd_buffer;
        std::deque<ImageFrame> image_buffer;

        std::filesystem::create_directories("all_pcd_body");
        std::filesystem::create_directories("all_image");
        std::filesystem::create_directories("colmap");
        std::filesystem::create_directories("depth");
        std::filesystem::create_directories("reproj");

        const uint64_t IMAGE_SYNC_THRESHOLD_NS = 500000000; // 0.5 seconds

        auto process_matched_pair = [&](const OdomFrame& odom, const PcdFrame& pcd) {
            // Gate: only output when a matching image exists (pose is the anchor)
            if (image_buffer.empty()) return;

            auto closest_it = image_buffer.begin();
            uint64_t min_diff = UINT64_MAX;
            for (auto it = image_buffer.begin(); it != image_buffer.end(); ++it) {
                uint64_t diff = std::abs((long long)it->timestamp - (long long)odom.timestamp);
                if (diff < min_diff) {
                    min_diff = diff;
                    closest_it = it;
                }
            }
            if (min_diff > IMAGE_SYNC_THRESHOLD_NS) return;

            std::ostringstream oss;
            double ts_sec = static_cast<double>(odom.timestamp) / 1e9;
            oss << std::fixed << std::setprecision(6) << ts_sec;
            std::string ts_name = oss.str();

            // Parse T_WL (Lidar in World) from odom
            Eigen::Quaterniond q_WL(odom.pose.qw, odom.pose.qx, odom.pose.qy, odom.pose.qz);
            Eigen::Matrix3d R_WL = q_WL.toRotationMatrix();
            Eigen::Vector3d t_WL(odom.pose.x, odom.pose.y, odom.pose.z);

            // Lidar to IMU Calibration (T_IL): P_IMU = R_IL * P_Lidar + t_IL
            Eigen::Matrix3d R_IL;
            R_IL << -0.000000, -1.000000, -0.000000,
                     0.500000, -0.000000, -0.866025,
                     0.866025, -0.000000,  0.500000;
            Eigen::Vector3d t_IL(-0.0, -0.0684313328845596, -0.008221943675581425);

            // Compute T_LI = T_IL^{-1}
            Eigen::Matrix3d R_LI = R_IL.transpose();
            Eigen::Vector3d t_LI = -R_IL.transpose() * t_IL;

            // Compute T_WI = T_WL * T_LI (IMU in World)
            Eigen::Matrix3d R_WI = R_WL * R_LI;
            Eigen::Vector3d t_WI = R_WL * t_LI + t_WL;
            Eigen::Quaterniond q_WI(R_WI);
            q_WI.normalize();

            // Save T_WI to lidar_poses.txt
            std::ofstream ofsl("all_pcd_body/lidar_poses.txt", std::ios::app);
            if (ofsl.is_open()) {
                ofsl << std::fixed << std::setprecision(10);
                ofsl << ts_name << " "
                    << t_WI.x() << " " << t_WI.y() << " " << t_WI.z() << " "
                    << q_WI.x() << " " << q_WI.y() << " " << q_WI.z() << " " << q_WI.w() << "\n";
            }

            // Save exactly the same T_WI to image_poses.txt
            std::ofstream ofs_cam("all_image/image_poses.txt", std::ios::app);
            if (ofs_cam.is_open()) {
                ofs_cam << std::fixed << std::setprecision(10);
                ofs_cam << ts_name << " "
                        << t_WI.x() << " " << t_WI.y() << " " << t_WI.z() << " "
                        << q_WI.x() << " " << q_WI.y() << " " << q_WI.z() << " " << q_WI.w() << "\n";
            }

            // Save image (raw compressed bytes, no decode)
            std::ofstream fout("all_image/" + ts_name + ".png", std::ios::binary);
            if (fout.is_open()) {
                fout.write(reinterpret_cast<const char*>(closest_it->data.data()), closest_it->data.size());
            }
            image_buffer.erase(closest_it);

            // Transform pointcloud from World to IMU frame (T_IW = T_WI^{-1})
            Eigen::Matrix3d R_IW = R_WI.transpose();
            Eigen::Vector3d t_IW = -R_WI.transpose() * t_WI;

            Eigen::Affine3d transform_IW = Eigen::Affine3d::Identity();
            transform_IW.translation() = t_IW;
            transform_IW.linear() = R_IW;

            PointCloud::Ptr transformed_cloud(new PointCloud());
            pcl::transformPointCloud(*(pcd.cloud), *transformed_cloud, transform_IW.cast<float>());

            // Save individual PCD in local IMU frame
            pcl::io::savePCDFileBinary("all_pcd_body/" + ts_name + ".pcd", *transformed_cloud);
        };

        auto try_sync = [&](bool flush_all = false) {
            while (!odom_buffer.empty() && !pcd_buffer.empty()) {
                if (!flush_all && (odom_buffer.size() < 2 || pcd_buffer.size() < 2)) break;

                auto& o1 = odom_buffer[0];
                auto& p1 = pcd_buffer[0];

                if (o1.timestamp < p1.timestamp) {
                    if (odom_buffer.size() > 1) {
                        auto& o2 = odom_buffer[1];
                        if (std::abs((long long)o2.timestamp - (long long)p1.timestamp) <=
                            std::abs((long long)o1.timestamp - (long long)p1.timestamp)) {
                            odom_buffer.pop_front();
                            continue;
                        }
                    } else if (!flush_all) break;
                    process_matched_pair(o1, p1);
                    odom_buffer.pop_front();
                    pcd_buffer.pop_front();
                } else {
                    if (pcd_buffer.size() > 1) {
                        auto& p2 = pcd_buffer[1];
                        if (std::abs((long long)p2.timestamp - (long long)o1.timestamp) <=
                            std::abs((long long)p1.timestamp - (long long)o1.timestamp)) {
                            pcd_buffer.pop_front();
                            continue;
                        }
                    } else if (!flush_all) break;
                    process_matched_pair(o1, p1);
                    odom_buffer.pop_front();
                    pcd_buffer.pop_front();
                }
            }

            // Phase 2: cleanup stale images that are too old for any remaining odom
            if (!odom_buffer.empty() && !image_buffer.empty()) {
                uint64_t oldest_odom_ts = odom_buffer.front().timestamp;
                while (!image_buffer.empty() &&
                       image_buffer.front().timestamp + IMAGE_SYNC_THRESHOLD_NS < oldest_odom_ts) {
                    image_buffer.pop_front();
                }
            }
        };

        while (true) {
            ZmqMessage msg;
            g_queue.pop(msg);

            if (msg.header == "[EOF]") {
                try_sync(true);
                break;
            }

            if (msg.header == "[ODOM]") {
                odom_buffer.push_back(parseOdomPayload(msg.payload.data(), msg.payload.size()));
                try_sync();
            } else if (msg.header == "[PCD]") {
                pcd_buffer.push_back(parseSensorPc2Payload(msg.payload.data(), msg.payload.size()));
                try_sync();
            } else if (msg.header == "[IMAGE]") {
                ImageFrame img = parseImagePayloadLight(msg.payload.data(), msg.payload.size());
                if (img.timestamp != 0) {
                    image_buffer.push_back(std::move(img));
                    try_sync();
                }
            }
        }
        receiver_thread.join();
        return 0;
    }
    return 0;
}
