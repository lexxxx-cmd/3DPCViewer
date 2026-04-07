#pragma once
#include <QImage>
#include <QString>
#include <vector>
#include <cstdint>
#include <QMetaType> // 必须包含

#include <array>
#include <cstdint>
#include <algorithm>

// 1. 定义 RGB 结构体
struct ColorRGB {
    uint8_t r, g, b;
};

// 2. 编译期生成 256 级 Jet 调色板
constexpr std::array<ColorRGB, 256> generateJetLUT() {
    std::array<ColorRGB, 256> lut{};
    for (int i = 0; i < 256; ++i) {
        float v = i / 255.0f;
        float r = 0.0f, g = 0.0f, b = 0.0f;

        if (v < 0.25f) {
            r = 0.0f; g = 4.0f * v; b = 1.0f;
        }
        else if (v < 0.5f) {
            r = 0.0f; g = 1.0f; b = 1.0f - 4.0f * (v - 0.25f);
        }
        else if (v < 0.75f) {
            r = 4.0f * (v - 0.5f); g = 1.0f; b = 0.0f;
        }
        else {
            r = 1.0f; g = 1.0f - 4.0f * (v - 0.75f); b = 0.0f;
        }

        lut[i].r = static_cast<uint8_t>(r * 255.0f);
        lut[i].g = static_cast<uint8_t>(g * 255.0f);
        lut[i].b = static_cast<uint8_t>(b * 255.0f);
    }
    return lut;
}

// 3. 声明全局/静态的查找表 (编译完成后，它就已经是一个写死在内存里的数组了)
constexpr auto JET_LUT = generateJetLUT();
// Livox 单个点结构（1字节对齐防止读取越界）
#pragma pack(push, 1) 
struct GeneralPointI {
    float x;
    float y;
    float z;
    uint8_t reflectivity;
};
#pragma pack(pop)

#pragma pack(push, 1) 
struct GeneralPointIRGB {
    GeneralPointI pointI;
    uint8_t r;
    uint8_t g;
    uint8_t b;
};
#pragma pack(pop)

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
struct GeneralCloudFrame {
    uint64_t timestamp;
    QString frame_id;
    std::vector<GeneralPointIRGB> points;
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
    uint64_t timestamp;// 纳秒级时间戳
    int index;
    QString frame_id;
    QString child_frame_id;
    Pose pose;
    Twist twist;
};

// 注册元类型，允许跨线程信号槽中传递
Q_DECLARE_METATYPE(GeneralCloudFrame)
Q_DECLARE_METATYPE(ImageFrame)
Q_DECLARE_METATYPE(OdomFrame)