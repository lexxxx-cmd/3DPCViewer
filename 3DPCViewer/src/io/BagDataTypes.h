#pragma once
#include <QImage>
#include <QString>
#include <QByteArray>
#include <vector>
#include <cstdint>
#include <QMetaType> // 元类型注册

#include <array>
#include <cstdint>
#include <algorithm>

// 1. ���� RGB �ṹ��
struct ColorRGB {
    uint8_t r, g, b;
};

// 2. ���������� 256 �� Jet ��ɫ��
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

// 3. ����ȫ��/��̬�Ĳ��ұ� (������ɺ������Ѿ���һ��д�����ڴ����������)
constexpr auto JET_LUT = generateJetLUT();
// Livox ������ṹ��1�ֽڶ����ֹ��ȡԽ�磩
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
// ����ǰ�˵�һ֡����
struct GeneralCloudFrame {
    uint64_t timestamp;
    QString frame_id;
    std::vector<GeneralPointIRGB> points;
};

// ����ǰ�˵�һ֡ͼ��
struct ImageFrame {
    uint64_t timestamp;
    QImage image;
};

struct Pose {
    double x, y, z;          // λ��
    double qx, qy, qz, qw;   // ��Ԫ����̬
};

// �ٶ���Ϣ
struct Twist {
    double linear_x, linear_y, linear_z;
    double angular_x, angular_y, angular_z;
};

// ����ǰ�˵�һ֡��̼�����
struct OdomFrame {
    uint64_t timestamp;// ���뼶ʱ���
    int index;
    QString frame_id;
    QString child_frame_id;
    Pose pose;
    Twist twist;
};

// 注册元类型，以便跨线程信号槽传递
Q_DECLARE_METATYPE(GeneralCloudFrame)
Q_DECLARE_METATYPE(ImageFrame)
Q_DECLARE_METATYPE(OdomFrame)

// ---------------------------------------------------------------------------
// RawBagMessage — cross-thread payload for the DB-write pipeline
// ---------------------------------------------------------------------------
// topicName  : original ROS topic, e.g. "/livox/lidar"
// typeName   : ROS message type, e.g. "sensor_msgs/PointCloud2"
// timestampNs: message timestamp in nanoseconds (from the bag record header)
// rawData    : DDS-CDR encoded blob produced by CDRConverter
// bagIndex   : 1-based counter identifying which imported .bag file this
//              message belongs to (set by DataService)
struct RawBagMessage {
    int        bagIndex{1};
    QString    topicName;
    QString    typeName;
    qint64     timestampNs{0};
    QByteArray rawData;
};

Q_DECLARE_METATYPE(RawBagMessage)