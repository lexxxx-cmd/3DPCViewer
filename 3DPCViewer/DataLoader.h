// DataLoader.h
#ifndef DATALOADER_H
#define DATALOADER_H

#include "CloudData.h"
#include <osgDB/ReadFile>
#include <osg/Geode>
#include <osg/Geometry>
#include <QFileInfo>
#include <fstream>
#include <sstream>

class DataLoader {
public:
    static CloudData loadFile(const QString& path) {
        CloudData data;
        QString ext = QFileInfo(path).suffix().toLower();

        if (ext == "pcd") {
            loadPCDManual(path, data);
        }
        else if (ext == "ply" || ext == "obj") {
            // 使用 OSG 原生插件读取
            osg::ref_ptr<osg::Node> node = osgDB::readNodeFile(path.toStdString());
            if (node.valid()) {
                extractPointsFromNode(node, data);
            }
        }
        return data;
    }

private:
    // ASCII PCD 
    static void loadPCDManual(const std::string& path, CloudData& data) {
        std::ifstream file(path);
        if (!file.is_open()) return;

        std::string line;
        bool dataSection = false;
        while (std::getline(file, line)) {
            if (line.find("DATA ascii") != std::string::npos) {
                dataSection = true;
                continue;
            }
            if (!dataSection || line.empty()) continue;

            std::stringstream ss(line);
            float x, y, z;
            if (ss >> x >> y >> z) {
                data.points.push_back(osg::Vec3(x, y, z));
            }
        }
    }
    // 二进制
    static void loadPCDManual(const QString& fileName, CloudData& data) {
        QFile file(fileName);
        if (!file.open(QIODevice::ReadOnly)) return;

        // 1. 读取头部
        QByteArray header;
        while (!file.atEnd()) {
            QByteArray line = file.readLine();
            header += line;
            if (line.startsWith("DATA")) break;
        }

        // 2. 解析头部信息
        int points = 0;
        bool hasRGB = false;
        QList<QByteArray> headerLines = header.split('\n');
        for (const auto& line : headerLines) {
            if (line.startsWith("POINTS")) {
                points = line.split(' ')[1].trimmed().toInt();
            }
            if (line.startsWith("FIELDS")) {
                hasRGB = line.contains("rgb");
            }
        }
        if (points == 0) return;

        // 3. 读取二进制数据
        int pointSize = hasRGB ? (sizeof(float) * 3 + sizeof(uint32_t)) : (sizeof(float) * 3);
        QByteArray bin = file.read(points * pointSize);
        const char* ptr = bin.constData();

        for (int i = 0; i < points; ++i) {
            float x = *reinterpret_cast<const float*>(ptr);
            float y = *reinterpret_cast<const float*>(ptr + 4);
            float z = *reinterpret_cast<const float*>(ptr + 8);
            data.points.push_back(osg::Vec3(x, y, z));
            if (hasRGB) {
                uint32_t rgb = *reinterpret_cast<const uint32_t*>(ptr + 12);
                float r = ((rgb >> 16) & 0xFF) / 255.0f;
                float g = ((rgb >> 8) & 0xFF) / 255.0f;
                float b = (rgb & 0xFF) / 255.0f;
                data.colors.push_back(osg::Vec4(r, g, b, 1.0f));
                ptr += 16;
            }
            else {
                ptr += 12;
            }
        }
        data.hasColor = hasRGB;
    }

    // 从 OSG 加载的节点中提取顶点数据
    static void extractPointsFromNode(osg::Node* node, CloudData& data) {
        // 使用访问器遍历节点获取所有 Geometry（此处为简化逻辑）
        struct PointExtractor : public osg::NodeVisitor {
            CloudData& _data;
            PointExtractor(CloudData& d) : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN), _data(d) {}
            void apply(osg::Geode& geode) override {
                for (unsigned int i = 0; i < geode.getNumDrawables(); ++i) {
                    osg::Geometry* geom = geode.getDrawable(i)->asGeometry();
                    if (geom && geom->getVertexArray()) {
                        osg::Vec3Array* vertices = dynamic_cast<osg::Vec3Array*>(geom->getVertexArray());
                        if (vertices) {
                            for (auto v : *vertices) _data.points.push_back(v);
                        }
                    }
                }
            }
        };
        PointExtractor visitor(data);
        node->accept(visitor);
    }
};

#endif