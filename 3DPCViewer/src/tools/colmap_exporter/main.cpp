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

struct Point3D {
    float x, y, z;
    uint8_t r, g, b;
};

struct CloudFrame {
    std::string frame_id;
    std::vector<Point3D> points;
};

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

CloudFrame parseSensorPc2Payload(const uint8_t* payload, size_t length) {
    CloudFrame frame;
    size_t offset = 0;

    offset += 12; // seq, sec, nsec
    uint32_t frame_id_len = *(uint32_t*)(payload + offset);
    offset += 4;
    frame.frame_id = std::string((const char*)(payload + offset), frame_id_len);
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

    frame.points.resize(point_num);
    for (uint32_t i = 0; i < point_num; ++i) {
        size_t base = i * point_step;
        if (x_idx >= 0) frame.points[i].x = *(float*)(data_ptr + base + fields[x_idx].offset);
        if (y_idx >= 0) frame.points[i].y = *(float*)(data_ptr + base + fields[y_idx].offset);
        if (z_idx >= 0) frame.points[i].z = *(float*)(data_ptr + base + fields[z_idx].offset);

        if (rgb_idx >= 0) {
            const uint8_t* color_ptr = data_ptr + base + fields[rgb_idx].offset;
            frame.points[i].b = color_ptr[0];
            frame.points[i].g = color_ptr[1];
            frame.points[i].r = color_ptr[2];
        } else {
            frame.points[i].r = 255;
            frame.points[i].g = 255;
            frame.points[i].b = 255;
        }
    }

    return frame;
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
        
        // Remove old files
        std::remove("images.bin");
        std::remove("points3D.bin");
        
        while (true) {
            ZmqMessage msg;
            g_queue.pop(msg);
            
            if (msg.header == "[EOF]") {
                break;
            }
            
            if (msg.header == "[ODOM]") {
                OdomFrame odom = parseOdomPayload(msg.payload.data(), msg.payload.size());
                
                // Convert C2W to W2C
                double qx = odom.pose.qx, qy = odom.pose.qy, qz = odom.pose.qz, qw = odom.pose.qw;
                double tx = odom.pose.x, ty = odom.pose.y, tz = odom.pose.z;

                // Rotation matrix R_C2W from quaternion
                double R00 = 1.0 - 2.0 * (qy*qy + qz*qz);
                double R01 = 2.0 * (qx*qy - qz*qw);
                double R02 = 2.0 * (qx*qz + qy*qw);
                double R10 = 2.0 * (qx*qy + qz*qw);
                double R11 = 1.0 - 2.0 * (qx*qx + qz*qz);
                double R12 = 2.0 * (qy*qz - qx*qw);
                double R20 = 2.0 * (qx*qz - qy*qw);
                double R21 = 2.0 * (qy*qz + qx*qw);
                double R22 = 1.0 - 2.0 * (qx*qx + qy*qy);

                // t_W2C = -R_C2W^T * t_C2W
                double t_w2c_x = -(R00 * tx + R10 * ty + R20 * tz);
                double t_w2c_y = -(R01 * tx + R11 * ty + R21 * tz);
                double t_w2c_z = -(R02 * tx + R12 * ty + R22 * tz);

                // q_W2C is conjugate of q_C2W
                double q_w2c_w = qw, q_w2c_x = -qx, q_w2c_y = -qy, q_w2c_z = -qz;

                // Write to images.bin
                uint32_t image_id = odom_count + 1;
                uint32_t camera_id = 1;
                uint64_t points2d_size = 0;

                // Name: 1774599813.6.jpg from 1774599813 and 667775041 (stamp.sec + '.' + first digit of nsec)
                uint32_t sec = odom.timestamp / 1000000000ULL;
                uint32_t nsec = odom.timestamp % 1000000000ULL;
                std::stringstream ss;
                ss << sec << "." << (nsec / 100000000) << ".jpg";
                std::string name = ss.str();

                std::ofstream ofs("images.bin", std::ios::binary | std::ios::app);
                if (ofs.is_open()) {
                    ofs.write((char*)&image_id, sizeof(uint32_t));
                    ofs.write((char*)&q_w2c_w, sizeof(double));
                    ofs.write((char*)&q_w2c_x, sizeof(double));
                    ofs.write((char*)&q_w2c_y, sizeof(double));
                    ofs.write((char*)&q_w2c_z, sizeof(double));
                    ofs.write((char*)&t_w2c_x, sizeof(double));
                    ofs.write((char*)&t_w2c_y, sizeof(double));
                    ofs.write((char*)&t_w2c_z, sizeof(double));
                    ofs.write((char*)&camera_id, sizeof(uint32_t));
                    ofs.write(name.c_str(), name.length() + 1); // include '\0'
                    ofs.write((char*)&points2d_size, sizeof(uint64_t));
                    ofs.close();
                }

                if (odom_count < 5) {
                    std::cout << "[ColmapExporter] Handled ODOM Frame " << odom_count + 1 
                              << " | Timestamp: " << odom.timestamp 
                              << " | Pose: (" << odom.pose.x << ", " << odom.pose.y << ", " << odom.pose.z << ")"
                              << std::endl;
                }
                odom_count++;
            }
            else if (msg.header == "[PCD]") {
                CloudFrame cloud = parseSensorPc2Payload(msg.payload.data(), msg.payload.size());
                
                if (pcd_count < 5) { // Just print first few frames to verify
                    std::cout << "[ColmapExporter] Handled PCD Frame " << pcd_count + 1 
                              << " | Frame ID: " << cloud.frame_id 
                              << " | Extracted Points: " << cloud.points.size() 
                              << std::endl;
                }
                pcd_count++;
            }
        }
        
        receiver_thread.join();
        std::cout << "[ColmapExporter] Processed " << odom_count << " ODOM frames." << std::endl;
        std::cout << "[ColmapExporter] Processed " << pcd_count << " PCD frames." << std::endl;
        return 0;
    }


    printf("ORB-SLAM3");
    return 0;
}