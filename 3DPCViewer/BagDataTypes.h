#pragma once
#include <QImage>
#include <QString>
#include <vector>
#include <cstdint>
#include <QMetaType> // 必须包含

// Livox 单个点结构（1字节对齐防止读取越界）
#pragma pack(push, 1) 
struct LivoxPoint {
    uint32_t offset_time;
    float x;
    float y;
    float z;
    uint8_t reflectivity;
    uint8_t tag;
    uint8_t line;
};
#pragma pack(pop)

// 传给前端的一帧点云
struct LivoxCloudFrame {
    uint64_t timestamp;
    QString frame_id;
    std::vector<LivoxPoint> points;
};

// 传给前端的一帧图像
struct ImageFrame {
    uint64_t timestamp;
    QImage image;
};

struct Pose {
    double x, y, z;          // 位置
    double qx, qy, qz, qw;   // 四元数姿态
};

// 速度信息
struct Twist {
    double linear_x, linear_y, linear_z;
    double angular_x, angular_y, angular_z;
};

// 传给前端的一帧里程计数据
struct OdomFrame {
    uint64_t timestamp;      // 纳秒级时间戳
    QString frame_id;
    QString child_frame_id;
    Pose pose;
    Twist twist;
};

// 注册元类型，允许跨线程信号槽中传递
Q_DECLARE_METATYPE(LivoxCloudFrame)
Q_DECLARE_METATYPE(ImageFrame)
Q_DECLARE_METATYPE(OdomFrame)