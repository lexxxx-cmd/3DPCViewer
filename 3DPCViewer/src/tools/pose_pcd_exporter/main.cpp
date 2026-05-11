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

        std::filesystem::create_directories("pcds");

        auto process_matched_pair = [&](const OdomFrame& odom, const PcdFrame& pcd) {
            std::ostringstream oss;
            double ts_sec = static_cast<double>(odom.timestamp) / 1e9;
            oss << std::fixed << std::setprecision(1) << ts_sec;
            std::string ts_name = oss.str();

            // Lidar pose
            std::ofstream ofsl("lidar_poses.txt", std::ios::app);
            if (ofsl.is_open()) {
                ofsl << std::fixed << std::setprecision(10);
                ofsl << ts_name << " "
                    << odom.pose.x << " " << odom.pose.y << " " << odom.pose.z << " "
                    << odom.pose.qx << " " << odom.pose.qy << " " << odom.pose.qz << " " << odom.pose.qw << "\n";
            }

            // Camera pose logic (adapted from colmap_exporter)
            Eigen::Quaterniond q_c2w(odom.pose.qw, odom.pose.qx, odom.pose.qy, odom.pose.qz);
            Eigen::Matrix3d R_l2w = q_c2w.toRotationMatrix();
            Eigen::Vector3d t_l2w(odom.pose.x, odom.pose.y, odom.pose.z);

            Eigen::Matrix3d R_2col;
            R_2col << 0, -1, 0, 0, 0, -1, 1, 0, 0;

            Eigen::Matrix3d R_l2c;
            R_l2c << -0.005197, -0.999974, -0.004912, 0.574600, 0.001034, -0.818434, 0.818418, -0.007076, 0.574580;
            Eigen::Vector3d t_l2c(0.001718, -0.245889, -0.039587);

            Eigen::Matrix3d R_w2l = R_l2w.transpose();
            Eigen::Vector3d t_w2l = -R_w2l * t_l2w;

            Eigen::Matrix3d R_w2c_ros = R_l2c * R_w2l;
            Eigen::Vector3d t_w2c_ros = R_l2c * t_w2l + t_l2c;

            Eigen::Matrix3d R_w2c_fpv = R_w2c_ros * R_2col.transpose();
            Eigen::Vector3d t_w2c_fpv = t_w2c_ros;
            /*
            Eigen::Matrix3d R_main2fpv;
            R_main2fpv << 0.99995829, -0.00092137, 0.00908729,
                0.00061308, 0.99942605, 0.03387016,
                -0.00911328, -0.03386318, 0.99938493;
            Eigen::Vector3d t_main2fpv(0.00833581, 0.06864927, 0.00932383);
            */
            Eigen::Matrix3d R_main2fpv;
            R_main2fpv << 1.0,0,0,0,1,0,0,0,1;
            Eigen::Vector3d t_main2fpv(0,0,0);

            Eigen::Matrix3d R_c2w_fpv = R_w2c_fpv.transpose();
            Eigen::Vector3d t_c2w_fpv = -R_c2w_fpv * t_w2c_fpv;

            Eigen::Matrix3d R_c2w_main = R_c2w_fpv * R_main2fpv;
            Eigen::Vector3d t_c2w_main = R_c2w_fpv * t_main2fpv + t_c2w_fpv;

            Eigen::Matrix3d R_c2w_main_ros = R_2col.transpose() * R_c2w_main;
            Eigen::Quaterniond q_c2w_main_ros(R_c2w_main_ros);
            q_c2w_main_ros.normalize();
            Eigen::Vector3d t_c2w_main_ros = R_2col.transpose() * t_c2w_main;

            static bool printed_l2c = false;
            if (!printed_l2c) {
                Eigen::Matrix3d Final_R_w2c = R_c2w_main_ros.transpose();
                Eigen::Vector3d Final_t_w2c = -Final_R_w2c * t_c2w_main_ros;

                Eigen::Matrix3d Final_R_l2c = Final_R_w2c * R_l2w;
                Eigen::Vector3d Final_t_l2c = Final_R_w2c * t_l2w + Final_t_w2c;

                std::cout << "=== Final Lidar to Camera Transform ===" << std::endl;
                std::cout << "R_l2c:\n" << Final_R_l2c << std::endl;
                std::cout << "t_l2c:\n" << Final_t_l2c.transpose() << std::endl;
                std::cout << "=======================================" << std::endl;
                printed_l2c = true;
            }

            std::ofstream ofs_cam("image_poses.txt", std::ios::app);
            if (ofs_cam.is_open()) {
                ofs_cam << std::fixed << std::setprecision(10);
                ofs_cam << ts_name << " "
                        << t_c2w_main_ros.x() << " " << t_c2w_main_ros.y() << " " << t_c2w_main_ros.z() << " "
                        << q_c2w_main_ros.x() << " " << q_c2w_main_ros.y() << " " << q_c2w_main_ros.z() << " " << q_c2w_main_ros.w() << "\n";
            }

            // Transform PCD to world frame using Lidar pose
            Eigen::Affine3d transform = Eigen::Affine3d::Identity();
            transform.translation() = t_l2w;
            transform.rotate(q_c2w);
            Eigen::Affine3f transform_f = transform.cast<float>();

            PointCloud::Ptr transformed_cloud(new PointCloud());
            pcl::transformPointCloud(*(pcd.cloud), *transformed_cloud, transform_f);

            // Save individual PCD
            pcl::io::savePCDFileBinary("pcds/" + ts_name + ".pcd", *transformed_cloud);
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
            }
        }
        receiver_thread.join();
        return 0;
    }
    return 0;
}
