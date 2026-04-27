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
#include <pcl/filters/voxel_grid.h>
#include <pcl/filters/filter.h>
#include <pcl/io/pcd_io.h>
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <opencv2/opencv.hpp>

// Define structures needed for parsing
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

using PointT = pcl::PointXYZRGB;
using PointCloud = pcl::PointCloud<PointT>;

// Thread-safe queue
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

// Parser logic from main workspace
OdomFrame parseOdomPayload(const uint8_t* payload, size_t length) {
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
    odom.frame_id = std::string((const char*)(payload + offset), frame_id_len);
    offset += frame_id_len;

    uint32_t child_frame_id_len = *(uint32_t*)(payload + offset);
    offset += 4;
    odom.child_frame_id = std::string((const char*)(payload + offset), child_frame_id_len);
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

    return odom;
}

PointCloud::Ptr parseSensorPc2Payload(const uint8_t* payload, size_t length) {
    PointCloud::Ptr cloud(new PointCloud());
    size_t offset = 0;

    offset += 12; // seq, sec, nsec
    uint32_t frame_id_len = *(uint32_t*)(payload + offset);
    offset += 4;
    std::string frame_id = std::string((const char*)(payload + offset), frame_id_len);

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

    int x_idx = -1, y_idx = -1, z_idx = -1, rgb_idx = -1;
    for (size_t i = 0; i < fields.size(); ++i) {
        if (fields[i].name == "x") x_idx = i;
        if (fields[i].name == "y") y_idx = i;
        if (fields[i].name == "z") z_idx = i;
        if (fields[i].name == "rgb" || fields[i].name == "rgba") rgb_idx = i;
    }

    if (data_len < point_num * point_step) {
        point_num = data_len / point_step;
    }

    cloud->points.resize(point_num);
    for (uint32_t i = 0; i < point_num; ++i) {
        size_t base = i * point_step;
        if (x_idx >= 0) cloud->points[i].x = *(float*)(data_ptr + base + fields[x_idx].offset);
        if (y_idx >= 0) cloud->points[i].y = *(float*)(data_ptr + base + fields[y_idx].offset);
        if (z_idx >= 0) cloud->points[i].z = *(float*)(data_ptr + base + fields[z_idx].offset);

        if (rgb_idx >= 0) {
            const uint8_t* color_ptr = data_ptr + base + fields[rgb_idx].offset;
            cloud->points[i].b = color_ptr[0];
            cloud->points[i].g = color_ptr[1];
            cloud->points[i].r = color_ptr[2];
        } else {
            cloud->points[i].r = 255;
            cloud->points[i].g = 255;
            cloud->points[i].b = 255;
        }
    }

    cloud->width = point_num;
    cloud->height = 1;
    cloud->is_dense = false;

    return cloud;
}

void parseImagePayload(const uint8_t* payload, size_t length, const std::string& save_dir) {
    size_t offset = 0;

    // --- 1. 数据解析 (基于 ROS CompressedImage 序列化结构) ---

    // 安全检查：确保头部基本长度足够
    if (length < 12) return;

    // 解析序列号 seq (4 bytes)
    uint32_t seq = *reinterpret_cast<const uint32_t*>(payload + offset);
    offset += 4;

    // 解析秒 sec (4 bytes)
    uint32_t sec = *reinterpret_cast<const uint32_t*>(payload + offset);
    offset += 4;

    // 解析纳秒 nsec (4 bytes)
    uint32_t nsec = *reinterpret_cast<const uint32_t*>(payload + offset);
    offset += 4;

    // 解析 frame_id (string: uint32 长度 + 字符数据)
    uint32_t frame_id_len = *reinterpret_cast<const uint32_t*>(payload + offset);
    offset += 4;
    offset += frame_id_len;

    // 解析 format 字符串 (string: uint32 长度 + 字符数据)
    uint32_t format_len = *reinterpret_cast<const uint32_t*>(payload + offset);
    offset += 4;
    std::string format_str(reinterpret_cast<const char*>(payload + offset), format_len);
    offset += format_len;

    // 获取图像数据长度 (uint8[]: uint32 长度 + 数据)
    uint32_t data_len = *reinterpret_cast<const uint32_t*>(payload + offset);
    offset += 4;

    // 越界检查
    if (offset + data_len > length) {
        std::cerr << "[Error] Payload data incomplete or out of bounds!" << std::endl;
        return;
    }

    // --- 2. 图像解码 ---

    // 构造 cv::Mat 包装原始压缩数据 (无内存拷贝)
    cv::Mat rawData(1, static_cast<int>(data_len), CV_8UC1, const_cast<uint8_t*>(payload + offset));
    cv::Mat decoded_image = cv::imdecode(rawData, cv::IMREAD_COLOR);

    if (decoded_image.empty()) {
        std::cerr << "[Error] Image decoding failed! Format: " << format_str << std::endl;
        return;
    }

    // --- 3. 保存图片逻辑 ---

    try {
        // 使用 C++17 filesystem 创建目录
        if (!std::filesystem::exists(save_dir)) {
            std::filesystem::create_directories(save_dir);
        }

        // 格式化文件名：sec.nsec.jpg
        // 使用 std::setfill('0') 确保纳秒位数为9位，方便按名称排序
        std::ostringstream oss;
        oss << sec << "." << std::setfill('0') << std::setw(9) << nsec << ".jpg";
        std::string filename = oss.str();

        // 拼接路径
        std::filesystem::path filePath = std::filesystem::path(save_dir) / filename;

        // 保存图片
        if (!cv::imwrite(filePath.string(), decoded_image)) {
            std::cerr << "[Error] Failed to save image to: " << filePath << std::endl;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "[Exception] Filesystem error: " << e.what() << std::endl;
    }
}
// Global queue
MessageQueue g_queue;

void receiver_thread_func(int port) {
    std::cout << "[ColmapExporter] Starting ZMQ receiver on port: " << port << std::endl;
    zmq::context_t context(1);
    zmq::socket_t socket(context, zmq::socket_type::pull);
    socket.connect("tcp://127.0.0.1:" + std::to_string(port));

    while (true) {
        zmq::message_t msg_head;
        auto res = socket.recv(msg_head, zmq::recv_flags::none);
        if (!res) continue;

        std::string head_str(static_cast<char*>(msg_head.data()), msg_head.size());
        
        if (head_str == "[EOF]") {
            std::cout << "[ColmapExporter] Received EOF. Shutting down receiver." << std::endl;
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
int main(int argc, char** argv)
{
    // Exporter mode logic
    if (argc >= 3 && std::string(argv[1]) == "--zmq-port") {
        int port = std::stoi(argv[2]);

        std::thread receiver_thread(receiver_thread_func, port);

        int odom_count = 0;
        int pcd_count = 0;
        int img_count = 0;

        PointCloud::Ptr global_cloud(new PointCloud());
        PointCloud::Ptr temp_downsampled_cloud(new PointCloud());
        float voxel_size = 0.05f;
        pcl::VoxelGrid<PointT> vg;
        vg.setLeafSize(voxel_size, voxel_size, voxel_size);
        const int32_t camera_id = 1;
        const uint64_t image_width = 1280;
        const uint64_t image_height = 960;
        // OPENCV 模型参数：fx, fy, cx, cy, k1, k2, p1, p2
        const double cam_params[8] = {
            641.48038,    // fx
            668.49735,    // fy
            640.79402,    // cx
            490.03617,    // cy
            0.182431,     // k1
            -0.258910,    // k2
            0.000324,     // p1
            -0.000791     // p2
        };

        // 只写入 cameras.txt
        {
            std::ofstream ofs_camera_txt("cameras.txt");
            if (ofs_camera_txt.is_open()) {
                // COLMAP 标准注释头
                ofs_camera_txt << "# Camera list with one line of data per camera:" << std::endl;
                ofs_camera_txt << "#   CAMERA_ID, MODEL, WIDTH, HEIGHT, PARAMS[]" << std::endl;
                ofs_camera_txt << "# Number of cameras: 1" << std::endl;

                // 浮点数保留15位精度（避免数据丢失）
                ofs_camera_txt << std::fixed << std::setprecision(15);

                // 写入格式：ID 模型 宽 高 fx fy cx cy k1 k2 p1 p2
                ofs_camera_txt << camera_id << " "
                    << "OPENCV" << " "
                    << image_width << " "
                    << image_height << " "
                    << cam_params[0] << " "  // fx
                    << cam_params[1] << " "  // fy
                    << cam_params[2] << " "  // cx
                    << cam_params[3] << " "  // cy
                    << cam_params[4] << " "  // k1
                    << cam_params[5] << " "  // k2
                    << cam_params[6] << " "  // p1
                    << cam_params[7] << std::endl;  // p2

                ofs_camera_txt.close();
                std::cout << "[ColmapExporter] cameras.txt completed!" << std::endl;
            }
        }
            while (true) {
                ZmqMessage msg;
                g_queue.pop(msg);

                if (msg.header == "[EOF]") {
                    break;
                }

                if (msg.header == "[ODOM]") {
                    OdomFrame odom = parseOdomPayload(msg.payload.data(), msg.payload.size());

                    // 1. 获取 ROS 坐标系下的 LiDAR C2W (Camera-to-World) 姿态与位置
                    Eigen::Quaterniond q_c2w(odom.pose.qw, odom.pose.qx, odom.pose.qy, odom.pose.qz);
                    Eigen::Matrix3d R_l2w = q_c2w.toRotationMatrix();
                    Eigen::Vector3d t_l2w(odom.pose.x, odom.pose.y, odom.pose.z);

                    // 2. 定义逆变换矩阵: ROS -> COLMAP
                    Eigen::Matrix3d R_2col;
                    R_2col << 0, -1, 0,
                        0, 0, -1,
                        1, 0, 0;
                    // lidar -> camera
                    Eigen::Matrix3d R_l2c;
                    R_l2c << -0.005197, -0.999974, -0.004912,
                        0.574600, 0.001034, -0.818434,
                        0.818418, -0.007076, 0.574580;
                    Eigen::Vector3d t_l2c(0.001718, -0.245889, -0.039587);  // 如果有具体的平移，请替换这里
                    // ROS 下camera c2w
                    Eigen::Matrix3d R_w2l = R_l2w.transpose();
                    Eigen::Vector3d t_w2l = -R_w2l * t_l2w;

                    Eigen::Matrix3d R_w2c_ros = R_l2c * R_w2l;
                    Eigen::Vector3d t_w2c_ros = R_l2c * t_w2l + t_l2c;

                    Eigen::Matrix3d R_w2c_matrix = R_w2c_ros * R_2col.transpose();
                    Eigen::Vector3d t_w2c = t_w2c_ros;
                    Eigen::Quaterniond q_w2c(R_w2c_matrix);
                    q_w2c.normalize();

                    // 5. 准备写入 images.bin
                    uint32_t image_id = odom_count + 1;
                    uint32_t camera_id = 1;
                    uint64_t points2d_size = 0;

                    // 提取安全的四元数和平移
                    double qw = q_w2c.w();
                    double qx = q_w2c.x();
                    double qy = q_w2c.y();
                    double qz = q_w2c.z();
                    double tx = t_w2c.x();
                    double ty = t_w2c.y();
                    double tz = t_w2c.z();

                    // 名称生成保持不变
                    uint32_t sec = odom.timestamp / 1000000000ULL;
                    uint32_t nsec = odom.timestamp % 1000000000ULL;
                    std::stringstream ss;
                    // 建议保留更多的小数位以防重名，或者直接使用原始 timestamp
                    ss << sec << "." << (nsec / 100000000) << ".jpg";
                    std::string name = ss.str();
                    std::ofstream ofs("images.txt", std::ios::app);

                    if (ofs.is_open()) {
                        // 设置浮点数输出精度，确保位姿不丢失精度
                        ofs << std::fixed << std::setprecision(10);

                        // 第 1 行: 基本信息和位姿
                        // IMAGE_ID, QW, QX, QY, QZ, TX, TY, TZ, CAMERA_ID, NAME
                        ofs << image_id << " "
                            << qw << " " << qx << " " << qy << " " << qz << " "
                            << tx << " " << ty << " " << tz << " "
                            << camera_id << " " << name << "\n";

                        // 第 2 行: 特征点数据
                        // 注意：即使没有特征点，COLMAP 格式也要求这一行存在（可以为空行）
                        // 如果你有具体的 2D 点坐标，应在此循环写入。目前按你原代码逻辑写入空行：
                        ofs << "\n";

                        ofs.close();
                    }
                }
                else if (msg.header == "[PCD]") {

                    PointCloud::Ptr cloud = parseSensorPc2Payload(msg.payload.data(), msg.payload.size());

                      // Debug: Save raw cloud to PCD for inspection

                    // 移除 NaN，并截断极端异常值点，防止 PCL VoxelGrid 计算包围盒时整型溢出
                    PointCloud::Ptr valid_cloud(new PointCloud());
                    valid_cloud->points.reserve(cloud->points.size());
                    for (const auto& pt : cloud->points) {
                        if (std::isfinite(pt.x) && std::isfinite(pt.y) && std::isfinite(pt.z) &&
                            std::abs(pt.x) < 1000.0f && std::abs(pt.y) < 1000.0f && std::abs(pt.z) < 1000.0f) {
                            valid_cloud->points.push_back(pt);
                        }
                    }
                    valid_cloud->width = valid_cloud->points.size();
                    valid_cloud->height = 1;
                    valid_cloud->is_dense = true;

                    vg.setInputCloud(valid_cloud);
                    vg.filter(*temp_downsampled_cloud);

                    *global_cloud += *temp_downsampled_cloud;

                    if (pcd_count < 5) { // Just print first few frames to verify
                        std::cout << "[ColmapExporter] Handled PCD Frame " << pcd_count + 1
                            << " | Extracted Points: " << cloud->size()
                            << " | Downsampled: " << temp_downsampled_cloud->size()
                            << std::endl;
                    }
                    pcd_count++;
                }
                else if (msg.header == "[IMAGE]") {
                    //解码保存文件夹
                    img_count++;
                    parseImagePayload(msg.payload.data(), msg.payload.size(),
                                      "./images");
                }
            }
            std::cout << "image: " << img_count;
            receiver_thread.join();

            // Final global downsample with adaptive voxel size to hit ~100MB target
            PointCloud::Ptr final_cloud(new PointCloud());
            pcl::VoxelGrid<PointT> vg_global;
            vg_global.setInputCloud(global_cloud);

            float final_voxel_size = voxel_size;
            // 51 bytes per point in points3D.bin (uint64 + 3x double + 3x uint8 + double + uint64)
            // 2,000,000 points * 51 bytes ~= 102 MB
            const size_t TARGET_POINTS = 2000000;

            vg_global.setLeafSize(final_voxel_size, final_voxel_size, final_voxel_size);
            vg_global.filter(*final_cloud);

            // Iteratively increase voxel size until the point count drops below the target
            while (final_cloud->size() > TARGET_POINTS) {
                final_voxel_size += 0.05f;
                std::cout << "[ColmapExporter] Global cloud size: " << final_cloud->size()
                    << " | Target: " << TARGET_POINTS
                    << " | Increasing voxel size to " << final_voxel_size << "m..." << std::endl;

                vg_global.setLeafSize(final_voxel_size, final_voxel_size, final_voxel_size);
                vg_global.filter(*final_cloud);

                // Fail-safe to avoid infinite loops if cloud doesn't shrink (e.g. huge voxel size limits)
                if (final_voxel_size > 2.0f) {
                    std::cout << "[ColmapExporter] Voxel size reached 2.0m, stopping adaptive downsample." << std::endl;
                    break;
                }
            }
            pcl::io::savePCDFileBinary(
                "debug_cloud.pcd",
                *final_cloud);
            std::cout << "[ColmapExporter] Final voxel size: " << final_voxel_size << "m" << std::endl;
            std::cout << "[ColmapExporter] Writing points3D.bin... Total points: " << final_cloud->size() << std::endl;

            // 建议文件名改为 .txt
            std::ofstream ofs_pts("points3D.txt");
            if (!ofs_pts.is_open()) {
                std::cerr << "Could not open points3D.txt for writing." << std::endl;
                return -1;
            }

            // COLMAP 文本文件通常包含一些注释头信息（可选，但建议加上）
            ofs_pts << "# 3D point list with one line of data per point:" << std::endl;
            ofs_pts << "#   POINT3D_ID, X, Y, Z, R, G, B, ERROR, TRACK[]" << std::endl;
            ofs_pts << "# Number of points: " << final_cloud->size() << std::endl;

            // 设置浮点数精度，防止坐标丢失精度
            ofs_pts << std::fixed << std::setprecision(8);

            uint64_t track_len = 0; // 轨迹长度为0，意味着这些点没有关联的 2D 特征点
            double error = 0.0;

            for (uint64_t i = 0; i < final_cloud->size(); ++i) {
                uint64_t point3d_id = i + 1;
                const auto& pt = final_cloud->points[i];
                // 坐标转换逻辑保持不变
                double pt_x = -pt.y;
                double pt_y = -pt.z;
                double pt_z = pt.y;

                // 写入格式：ID X Y Z R G B ERROR
                // 注意：R G B 必须转为 int，否则会输出不可见字符
                ofs_pts << point3d_id << " "
                    << pt_x << " "
                    << pt_y << " "
                    << pt_z << " "
                    << (int)pt.r << " "
                    << (int)pt.g << " "
                    << (int)pt.b << " "
                    << error;

                // TRACK 字段：由于 track_len 为 0，后面不接任何数据，直接换行
                // 如果以后有追踪数据，格式为 [IMAGE_ID POINT2D_IDX IMAGE_ID POINT2D_IDX ...]
                ofs_pts << std::endl;
            }

            ofs_pts.close();
            std::cout << "[ColmapExporter] points3D.txt completed!" << std::endl;

            std::cout << "[ColmapExporter] Processed " << odom_count << " ODOM frames." << std::endl;
            std::cout << "[ColmapExporter] Processed " << pcd_count << " PCD frames." << std::endl;
            return 0;
        }

    return 0;
}