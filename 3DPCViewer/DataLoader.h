#ifndef DATALOADER_H
#define DATALOADER_H

#include <osg/Geometry>
#include <osg/Array>
#include <osg/Vec3>
#include <osg/Vec4>
#include <QString>

#include <pcl/PCLPointCloud2.h>
#include <pcl/io/pcd_io.h>
#include <pcl/io/ply_io.h>
#include <pcl/io/obj_io.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/conversions.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/features/normal_3d.h>
#include <pcl/search/kdtree.h>
#include <pcl/console/print.h>

#include "BagDataTypes.h"

class DataLoader {
public:
    // 通用读取
    static pcl::PCLPointCloud2::Ptr loadGenericPointCloud(const QString& filepath) {
        std::string filename = filepath.toLocal8Bit().constData();
        pcl::PCLPointCloud2::Ptr cloud(new pcl::PCLPointCloud2());
        std::string extension = filename.substr(filename.find_last_of('.') + 1);
        std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

        if (extension == "pcd") {
            pcl::io::loadPCDFile(filename, *cloud);
        }
        else if (extension == "ply") {
            pcl::io::loadPLYFile(filename, *cloud);
        }
        else if (extension == "obj") {
            pcl::PolygonMesh mesh;
            pcl::io::loadOBJFile(filename, mesh);
            *cloud = mesh.cloud;
        }
        else if (extension == "bag") {

        }
        else {
            PCL_ERROR("不支持的文件格式: .%s\n", extension.c_str());
        }
        return cloud;
    }

    // 通用格式转osg
    static osg::ref_ptr<osg::Geometry> convertToOsgGeometry(const pcl::PCLPointCloud2::Ptr& cloud2) {
        osg::ref_ptr<osg::Geometry> geom = new osg::Geometry();
        if (!cloud2 || cloud2->data.empty()) return geom;

        // RGB
        bool has_rgb = false;
        for (const auto& field : cloud2->fields) {
            if (field.name == "rgb" || field.name == "rgba") { has_rgb = true; break; }
        }

        osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();
        osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();

        if (has_rgb) {
            pcl::PointCloud<pcl::PointXYZRGB> cloud;
            pcl::fromPCLPointCloud2(*cloud2, cloud);
            vertices->reserve(cloud.size());
            colors->reserve(cloud.size());
            for (const auto& pt : cloud.points) {
                vertices->push_back(osg::Vec3(pt.x, pt.y, pt.z));
                colors->push_back(osg::Vec4(pt.r / 255.0f, pt.g / 255.0f, pt.b / 255.0f, 1.0f));
            }
            geom->setColorArray(colors, osg::Array::BIND_PER_VERTEX);
        }
        else {
            pcl::PointCloud<pcl::PointXYZ> cloud;
            pcl::fromPCLPointCloud2(*cloud2, cloud);
            vertices->reserve(cloud.size());
            for (const auto& pt : cloud.points) {
                vertices->push_back(osg::Vec3(pt.x, pt.y, pt.z));
            }
            colors->push_back(osg::Vec4(0.0f, 1.0f, 1.0f, 1.0f));
            geom->setColorArray(colors, osg::Array::BIND_OVERALL);
        }

        geom->setVertexArray(vertices);
        geom->addPrimitiveSet(new osg::DrawArrays(GL_POINTS, 0, vertices->size()));
        return geom;
    }
    static osg::ref_ptr<osg::Geometry> convertToOsgGeometry(const LivoxCloudFrame& cloud2) {
        osg::ref_ptr<osg::Geometry> geom = new osg::Geometry();
        if (cloud2.points.empty()) return geom;

        // RGB
        bool has_rgb = false;
        for (const auto& field : cloud2.points) {
            if (field.reflectivity) { has_rgb = true; break; }
        }

        osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();
        osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();

        if (has_rgb) {
            pcl::PointCloud<pcl::PointXYZRGB> cloud;
            vertices->reserve(cloud2.points.size());
            colors->reserve(cloud2.points.size());
            for (const auto& pt : cloud2.points) {
                vertices->push_back(osg::Vec3(pt.x, pt.y, pt.z));
                colors->push_back(osg::Vec4(pt.reflectivity / 255.0f, pt.reflectivity / 255.0f, pt.reflectivity / 255.0f, 1.0f));
            }
            geom->setColorArray(colors, osg::Array::BIND_PER_VERTEX);
        }
        else {
            pcl::PointCloud<pcl::PointXYZ> cloud;
            vertices->reserve(cloud2.points.size());
            for (const auto& pt : cloud2.points) {
                vertices->push_back(osg::Vec3(pt.x, pt.y, pt.z));
            }
            colors->push_back(osg::Vec4(0.0f, 1.0f, 1.0f, 1.0f));
            geom->setColorArray(colors, osg::Array::BIND_OVERALL);
        }

        geom->setVertexArray(vertices);
        geom->addPrimitiveSet(new osg::DrawArrays(GL_POINTS, 0, vertices->size()));
        return geom;
    }

    // 采样
    static pcl::PCLPointCloud2::Ptr downsampleCloud(const pcl::PCLPointCloud2::Ptr& cloud2, float leaf_size = 0.01f) {
        pcl::PCLPointCloud2::Ptr cloud_filtered(new pcl::PCLPointCloud2());
        if (!cloud2 || cloud2->data.empty()) return cloud_filtered;

        pcl::VoxelGrid<pcl::PCLPointCloud2> sor;
        sor.setInputCloud(cloud2);
        sor.setLeafSize(leaf_size, leaf_size, leaf_size);
        sor.filter(*cloud_filtered);
        return cloud_filtered;
    }

    // 法线osg
    static osg::ref_ptr<osg::Geometry> computeNormalsAndConvert(const pcl::PCLPointCloud2::Ptr& cloud2) {
        osg::ref_ptr<osg::Geometry> geom = new osg::Geometry();
        if (!cloud2 || cloud2->data.empty()) return geom;


        pcl::PointCloud<pcl::PointXYZ>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZ>);
        pcl::fromPCLPointCloud2(*cloud2, *cloud);
        std::vector<int> indices;
        pcl::removeNaNFromPointCloud(*cloud, *cloud, indices);
        cloud->erase(
            std::remove_if(cloud->begin(), cloud->end(), [](const pcl::PointXYZ& pt) {
                return !pcl::isFinite(pt);
                }),
            cloud->end()
        );
        // 点数不足无法计算法线
        if (cloud->size() < 1000) return geom;

        pcl::NormalEstimation<pcl::PointXYZ, pcl::Normal> ne;
        ne.setInputCloud(cloud);
        pcl::search::KdTree<pcl::PointXYZ>::Ptr tree(new pcl::search::KdTree<pcl::PointXYZ>());
        ne.setSearchMethod(tree);
        pcl::PointCloud<pcl::Normal>::Ptr normals(new pcl::PointCloud<pcl::Normal>);
        ne.setKSearch(15);
        ne.compute(*normals);

        osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();
        osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
        colors->push_back(osg::Vec4(1.0f, 1.0f, 0.0f, 1.0f));
        geom->setColorArray(colors, osg::Array::BIND_OVERALL);

        float normal_length = 0.1f;
        for (size_t i = 0; i < cloud->size(); i += 5) {
            if (!std::isnan(normals->points[i].normal_x)) {
                osg::Vec3 pt(cloud->points[i].x, cloud->points[i].y, cloud->points[i].z);
                osg::Vec3 dir(normals->points[i].normal_x, normals->points[i].normal_y, normals->points[i].normal_z);
                vertices->push_back(pt);
                vertices->push_back(pt + dir * normal_length);
            }
        }
        geom->setVertexArray(vertices);
        geom->addPrimitiveSet(new osg::DrawArrays(GL_LINES, 0, vertices->size()));
        return geom;
    }
};

#endif