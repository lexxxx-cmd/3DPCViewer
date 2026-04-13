#pragma once

#include <QImage>
#include <QString>
#include <vector>
#include <cstdint>
#include <QMetaType>

#include <array>
#include <algorithm>

struct ColorRGB {
  uint8_t r, g, b;
};

constexpr std::array<ColorRGB, 256> GenerateJetLut() {
  std::array<ColorRGB, 256> lut{};
  for (int i = 0; i < 256; ++i) {
    float v = i / 255.0f;
    float r = 0.0f, g = 0.0f, b = 0.0f;

    if (v < 0.25f) {
      r = 0.0f; g = 4.0f * v; b = 1.0f;
    } else if (v < 0.5f) {
      r = 0.0f; g = 1.0f; b = 1.0f - 4.0f * (v - 0.25f);
    } else if (v < 0.75f) {
      r = 4.0f * (v - 0.5f); g = 1.0f; b = 0.0f;
    } else {
      r = 1.0f; g = 1.0f - 4.0f * (v - 0.75f); b = 0.0f;
    }

    lut[i].r = static_cast<uint8_t>(r * 255.0f);
    lut[i].g = static_cast<uint8_t>(g * 255.0f);
    lut[i].b = static_cast<uint8_t>(b * 255.0f);
  }
  return lut;
}

constexpr auto kJetLut = GenerateJetLut();

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
  GeneralPointI point_i;
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

struct GeneralCloudFrame {
  uint64_t timestamp;
  QString frame_id;
  std::vector<GeneralPointIRGB> points;
};

struct ImageFrame {
  uint64_t timestamp;
  QImage image;
};

struct Pose {
  double x, y, z;
  double qx, qy, qz, qw;
};

struct Twist {
  double linear_x, linear_y, linear_z;
  double angular_x, angular_y, angular_z;
};

struct OdomFrame {
  uint64_t timestamp;
  int index;
  QString frame_id;
  QString child_frame_id;
  Pose pose;
  Twist twist;
};

Q_DECLARE_METATYPE(GeneralCloudFrame)
Q_DECLARE_METATYPE(ImageFrame)
Q_DECLARE_METATYPE(OdomFrame)
