// 1. CloudData.h
#ifndef CLOUDDATA_H
#define CLOUDDATA_H

#include <osg/Vec3>
#include <osg/Vec4>
#include <vector>
#include <string>

// 统一的点云数据交换格式
struct CloudData {
    std::vector<osg::Vec3> points;   // 坐标
    std::vector<osg::Vec4> colors;   // 颜色 (RGBA)
    std::vector<osg::Vec3> normals;  // 法线
    bool hasColor = false;
    bool hasNormal = false;

    void clear() {
        points.clear();
        colors.clear();
        normals.clear();
        hasColor = false;
        hasNormal = false;
    }
};

#endif