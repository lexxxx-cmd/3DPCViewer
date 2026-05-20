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
#include <deque>

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

struct ImageFrame {
    uint64_t timestamp;
    std::string filename;
};

// 修改 parseImagePayload 返回 ImageFrame
ImageFrame parseImagePayload(const uint8_t* payload, size_t length, const std::string& save_dir) {
    ImageFrame img_frame;
    img_frame.timestamp = 0; // 默认0表示解析失败
    size_t offset = 0;

    if (length < 12) return img_frame;

    uint32_t seq = *reinterpret_cast<const uint32_t*>(payload + offset); offset += 4;
    uint32_t sec = *reinterpret_cast<const uint32_t*>(payload + offset); offset += 4;
    uint32_t nsec = *reinterpret_cast<const uint32_t*>(payload + offset); offset += 4;

    // 提取确切的时间戳
    img_frame.timestamp = (uint64_t)sec * 1000000000ULL + nsec;

    uint32_t frame_id_len = *reinterpret_cast<const uint32_t*>(payload + offset); offset += 4; offset += frame_id_len;
    uint32_t format_len = *reinterpret_cast<const uint32_t*>(payload + offset); offset += 4;
    std::string format_str(reinterpret_cast<const char*>(payload + offset), format_len); offset += format_len;
    uint32_t data_len = *reinterpret_cast<const uint32_t*>(payload + offset); offset += 4;

    if (offset + data_len > length) return img_frame;

    cv::Mat rawData(1, static_cast<int>(data_len), CV_8UC1, const_cast<uint8_t*>(payload + offset));
    cv::Mat decoded_image = cv::imdecode(rawData, cv::IMREAD_COLOR);

    if (decoded_image.empty()) return img_frame;

    try {
        if (!std::filesystem::exists(save_dir)) std::filesystem::create_directories(save_dir);

        std::ostringstream oss;
        double time_sec = static_cast<double>(sec) + static_cast<double>(nsec) * 1e-9;
        oss << std::fixed << std::setprecision(1) << time_sec << ".jpg";
        img_frame.filename = oss.str(); // 记录文件名

        std::filesystem::path filePath = std::filesystem::path(save_dir) / img_frame.filename;
        cv::imwrite(filePath.string(), decoded_image);
    }
    catch (const std::exception& e) {
        std::cerr << "[Exception] Filesystem error: " << e.what() << std::endl;
    }

    return img_frame;
}

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

        std::deque<OdomFrame> odom_buffer;
        std::deque<ImageFrame> image_buffer;
        uint32_t synced_image_id = 1;



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
        auto process_matched_pair = [&](const OdomFrame& odom, const ImageFrame& img) {
            // 你的坐标转换逻辑（保持不变）
            Eigen::Quaterniond q_c2w(odom.pose.qw, odom.pose.qx, odom.pose.qy, odom.pose.qz);
            Eigen::Matrix3d R_l2w = q_c2w.toRotationMatrix();
            Eigen::Vector3d t_l2w(odom.pose.x, odom.pose.y, odom.pose.z);

            Eigen::Matrix3d R_l2c;
            R_l2c << -0.005197, -0.999974, -0.004912,
                0.574600, 0.001034, -0.818434,
                0.818418, -0.007076, 0.574580;
            Eigen::Vector3d t_l2c(0.001718, -0.245889, -0.039587);

            Eigen::Matrix3d R_w2l = R_l2w.transpose();
            Eigen::Vector3d t_w2l = -R_w2l * t_l2w;

            Eigen::Matrix3d R_w2c_ros = R_l2c * R_w2l;
            Eigen::Vector3d t_w2c_ros = R_l2c * t_w2l + t_l2c;

            Eigen::Matrix3d R_w2c_fpv = R_w2c_ros;
            Eigen::Vector3d t_w2c_fpv = t_w2c_ros;

            Eigen::Matrix3d R_main2fpv;
            R_main2fpv << 0.99995829, -0.00092137, 0.00908729,
                0.00061308, 0.99942605, 0.03387016,
                -0.00911328, -0.03386318, 0.99938493;
            Eigen::Vector3d t_main2fpv(0.00833581, 0.06864927, 0.00932383);

            // 将 FPV 的 W2C 转换为 C2W，以便在正确的相机坐标系下进行刚体级联
            Eigen::Matrix3d R_c2w_fpv = R_w2c_fpv.transpose();
            Eigen::Vector3d t_c2w_fpv = -R_c2w_fpv * t_w2c_fpv;

            // 计算 Main 的绝对 C2W 位姿: T_c2w_main = T_c2w_fpv * T_main2fpv
            Eigen::Matrix3d R_c2w_main = R_c2w_fpv * R_main2fpv;
            Eigen::Vector3d t_c2w_main = R_c2w_fpv * t_main2fpv + t_c2w_fpv;

            // 将最终 Main 的 C2W 重新求逆，转回 COLMAP 需要的 W2C 格式
            Eigen::Matrix3d R_w2c_main = R_c2w_main.transpose();
            Eigen::Vector3d t_w2c_main = -R_w2c_main * t_c2w_main;

            Eigen::Quaterniond q_w2c_main(R_w2c_main);
            q_w2c_main.normalize(); // 必须重新归一化以防止浮点误差导致四元数非法

            std::ofstream ofs("images.txt", std::ios::app);
            if (ofs.is_open()) {
                ofs << std::fixed << std::setprecision(10);
                // 使用同步后的 IMAGE_ID 和 图片的文件名
                ofs << synced_image_id++ << " "
                    << q_w2c_main.w() << " " << q_w2c_main.x() << " " << q_w2c_main.y() << " " << q_w2c_main.z() << " "
                    << t_w2c_main.x() << " " << t_w2c_main.y() << " " << t_w2c_main.z() << " "
                    << camera_id << " " << img.filename << "\n\n";
                ofs.close();
            }
            };
        auto try_sync = [&](bool flush_all = false) {
            while (!odom_buffer.empty() && !image_buffer.empty()) {
                // 如果不处于最后清空阶段，我们需要队列中至少有一个未来元素，
                // 这样才能确保当前的匹配是绝对最近的（越过谷底）
                if (!flush_all && (odom_buffer.size() < 2 || image_buffer.size() < 2)) {
                    break;
                }

                auto& o1 = odom_buffer[0];
                auto& i1 = image_buffer[0];

                // 判定哪一边更旧，并尝试拿更新的元素来比较
                if (o1.timestamp < i1.timestamp) {
                    if (odom_buffer.size() > 1) {
                        auto& o2 = odom_buffer[1];
                        // 如果下一个 ODOM 离当前 IMAGE 更近，说明当前的 ODOM 太旧了，丢弃它
                        if (std::abs((long long)o2.timestamp - (long long)i1.timestamp) <=
                            std::abs((long long)o1.timestamp - (long long)i1.timestamp)) {
                            odom_buffer.pop_front();
                            continue;
                        }
                    }
                    else if (!flush_all) {
                        break; // 等待下一个 ODOM 进行确认
                    }
                    // 匹配成功
                    process_matched_pair(o1, i1);
                    odom_buffer.pop_front();
                    image_buffer.pop_front();
                }
                else {
                    if (image_buffer.size() > 1) {
                        auto& i2 = image_buffer[1];
                        // 如果下一张 IMAGE 离当前 ODOM 更近，说明当前的 IMAGE 太旧了，丢弃
                        if (std::abs((long long)i2.timestamp - (long long)o1.timestamp) <=
                            std::abs((long long)i1.timestamp - (long long)o1.timestamp)) {
                            image_buffer.pop_front();
                            continue;
                        }
                    }
                    else if (!flush_all) {
                        break; // 等待下一张 IMAGE 进行确认
                    }
                    // 匹配成功
                    process_matched_pair(o1, i1);
                    odom_buffer.pop_front();
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
                odom_count++;
                try_sync();
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
                ImageFrame img = parseImagePayload(msg.payload.data(), msg.payload.size(), "./images");
                if (img.timestamp != 0) {
                    image_buffer.push_back(img);
                    img_count++;
                    try_sync(); // 每次进数据都尝试同步
                }
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

            double pt_x = pt.x;
            double pt_y = pt.y;
            double pt_z = pt.z;

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