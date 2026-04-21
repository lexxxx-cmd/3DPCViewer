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

        PointCloud::Ptr global_cloud(new PointCloud());
        PointCloud::Ptr temp_downsampled_cloud(new PointCloud());
        float voxel_size = 0.05f;
        pcl::VoxelGrid<PointT> vg;
        vg.setLeafSize(voxel_size, voxel_size, voxel_size);

        // Remove old files
        std::remove("images.bin");
        std::remove("points3D.bin");

        // 先写入占位的 num_images (稍后回填)
        uint64_t placeholder = 0;
        std::ofstream ofs_init("images.bin", std::ios::binary);
        ofs_init.write((char*)&placeholder, sizeof(uint64_t));
        ofs_init.close();

        // ===================== 修正为 SIMPLE_RADIAL 相机参数 =====================
        const int32_t camera_id = 1;
        const int32_t camera_model_id = 4;
        const uint64_t image_width = 3840;
        const uint64_t image_height = 2160;
        // SIMPLE_RADIAL 固定4个参数：f, cx, cy, k
        const double cam_params[4] = {
            3279.2218277699917,    // f (焦距)
            1920.0,                // cx (主点x)
            1080.0,                // cy (主点y)
            0.0016164597261278581  // k (径向畸变系数)
        };

        // 写入 cameras.bin (一次性写入，固定相机参数)
        std::ofstream ofs_camera("cameras.bin", std::ios::binary);
        if (ofs_camera.is_open()) {
            const uint64_t num_cameras = 1;
            ofs_camera.write((char*)&num_cameras, sizeof(uint64_t));

            ofs_camera.write((char*)&camera_id, sizeof(int32_t));
            ofs_camera.write((char*)&camera_model_id, sizeof(int32_t));
            ofs_camera.write((char*)&image_width, sizeof(uint64_t));
            ofs_camera.write((char*)&image_height, sizeof(uint64_t));

            // 写入 4 个 double 类型参数（SIMPLE_RADIAL 固定4个）
            for (int i = 0; i < 4; ++i) {
                ofs_camera.write((char*)&cam_params[i], sizeof(double));
            }
            ofs_camera.close();
            std::cout << "[ColmapExporter] cameras.bin completed!" << std::endl;

            std::ofstream ofs_camera_txt("cameras.txt");
            if (ofs_camera_txt.is_open()) {
                // COLMAP 标准注释头
                ofs_camera_txt << "# Camera list with one line of data per camera:" << std::endl;
                ofs_camera_txt << "#   CAMERA_ID, MODEL, WIDTH, HEIGHT, PARAMS[]" << std::endl;
                ofs_camera_txt << "# Number of cameras: 1" << std::endl;

                // 浮点数保留15位精度（避免数据丢失）
                ofs_camera_txt << std::fixed << std::setprecision(15);

                // 写入格式：ID 模型 宽 高 f cx cy k
                ofs_camera_txt << camera_id << " "
                    << "SIMPLE_RADIAL" << " "
                    << image_width << " "
                    << image_height << " "
                    << cam_params[0] << " "  // f
                    << cam_params[1] << " "  // cx
                    << cam_params[2] << " "  // cy
                    << cam_params[3] << std::endl;  // k

                ofs_camera_txt.close();
                std::cout << "[ColmapExporter] cameras.txt completed!" << std::endl;
            }
            while (true) {
                ZmqMessage msg;
                g_queue.pop(msg);

                if (msg.header == "[EOF]") {
                    break;
                }

                if (msg.header == "[ODOM]") {
                    OdomFrame odom = parseOdomPayload(msg.payload.data(), msg.payload.size());

                    // 1. 获取 OSG 坐标系下的 C2W (Camera-to-World) 姿态与位置
                    Eigen::Quaterniond q_osg(odom.pose.qw, odom.pose.qx, odom.pose.qy, odom.pose.qz);
                    Eigen::Matrix3d R_osg = q_osg.toRotationMatrix();
                    Eigen::Vector3d C_osg(odom.pose.x, odom.pose.y, odom.pose.z);

                    // 2. 定义逆变换矩阵: OSG -> COLMAP
                    // 在 processBin 中，COLMAP -> OSG 用的是 Rx_m90 (绕X旋转-90度)
                    // 因此这里 OSG -> COLMAP 需要用 Rx_m90 的逆，即 Rx_90 (绕X旋转+90度)
                    Eigen::Matrix3d Rx_90;
                    Rx_90 << 1, 0, 0,
                        0, 0, -1,
                        0, 1, 0;

                    // 3. 变换回 COLMAP 世界坐标系下的 C2W
                    // 严格反推: 既然 R_osg = Rx_m90 * R_c2w_colmap
                    // 那么 R_c2w_colmap = Rx_90 * R_osg
                    Eigen::Matrix3d R_c2w_colmap = Rx_90 * R_osg;
                    Eigen::Vector3d C_colmap = Rx_90 * C_osg;

                    // 4. 从 C2W 转化为 COLMAP 要求的 W2C (World-to-Camera)
                    Eigen::Matrix3d R_w2c = R_c2w_colmap.transpose();
                    Eigen::Vector3d t_w2c = -R_w2c * C_colmap;
                    Eigen::Quaterniond q_w2c(R_w2c);

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
                    ss << sec << "." << (nsec / 100000000) << ".jpg";
                    std::string name = ss.str();

                    // --- 性能优化：在循环外打开/关闭 images.bin 更好，这里按你原样写入 ---
                    std::ofstream ofs("images.bin", std::ios::binary | std::ios::app);
                    if (ofs.is_open()) {
                        ofs.write((char*)&image_id, sizeof(uint32_t));
                        ofs.write((char*)&qw, sizeof(double));
                        ofs.write((char*)&qx, sizeof(double));
                        ofs.write((char*)&qy, sizeof(double));
                        ofs.write((char*)&qz, sizeof(double));
                        ofs.write((char*)&tx, sizeof(double));
                        ofs.write((char*)&ty, sizeof(double));
                        ofs.write((char*)&tz, sizeof(double));
                        ofs.write((char*)&camera_id, sizeof(uint32_t));
                        ofs.write(name.c_str(), name.length() + 1); // include '\0'
                        ofs.write((char*)&points2d_size, sizeof(uint64_t));
                        ofs.close();
                    }

                    if (odom_count < 5) {
                        std::cout << "[ColmapExporter] Handled ODOM Frame " << odom_count + 1
                            << " | Timestamp: " << odom.timestamp
                            << " | Pose: (" << C_osg.x() << ", " << C_osg.y() << ", " << C_osg.z() << ")"
                            << std::endl;
                    }
                    odom_count++;
                }
                else if (msg.header == "[PCD]") {

                    PointCloud::Ptr cloud = parseSensorPc2Payload(msg.payload.data(), msg.payload.size());

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
            }

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

            std::cout << "[ColmapExporter] Final voxel size: " << final_voxel_size << "m" << std::endl;
            std::cout << "[ColmapExporter] Writing points3D.bin... Total points: " << final_cloud->size() << std::endl;

            std::ofstream ofs_pts("points3D.bin", std::ios::binary);
            uint64_t num_points = final_cloud->size();
            ofs_pts.write((char*)&num_points, sizeof(uint64_t));

            uint64_t track_len = 0;
            double error = 0.0;
            for (uint64_t i = 0; i < num_points; ++i) {
                uint64_t point3d_id = i + 1;
                const auto& pt = final_cloud->points[i];

                // Convert point from OSG to COLMAP (Rx_90)
                double pt_x = pt.x;
                double pt_y = -pt.z;
                double pt_z = pt.y;

                uint8_t pt_r = pt.r, pt_g = pt.g, pt_b = pt.b;

                ofs_pts.write((char*)&point3d_id, sizeof(uint64_t));
                ofs_pts.write((char*)&pt_x, sizeof(double));
                ofs_pts.write((char*)&pt_y, sizeof(double));
                ofs_pts.write((char*)&pt_z, sizeof(double));
                ofs_pts.write((char*)&pt_r, sizeof(uint8_t));
                ofs_pts.write((char*)&pt_g, sizeof(uint8_t));
                ofs_pts.write((char*)&pt_b, sizeof(uint8_t));
                ofs_pts.write((char*)&error, sizeof(double));
                ofs_pts.write((char*)&track_len, sizeof(uint64_t));
            }
            ofs_pts.close();

            // 回填 num_images 到文件开头
            std::fstream ofs_fix("images.bin", std::ios::binary | std::ios::in | std::ios::out);
            uint64_t final_count = static_cast<uint64_t>(odom_count);
            ofs_fix.seekp(0, std::ios::beg);
            ofs_fix.write((char*)&final_count, sizeof(uint64_t));
            ofs_fix.close();

            std::cout << "[ColmapExporter] Processed " << odom_count << " ODOM frames." << std::endl;
            std::cout << "[ColmapExporter] Processed " << pcd_count << " PCD frames." << std::endl;
            return 0;
        }

    }
    printf("ORB-SLAM3");
    return 0;
}