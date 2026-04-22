#pragma once

#include <osg/Group>
#include <osg/Geode>
#include <osg/ShapeDrawable>
#include <osg/Material>
#include <osg/PositionAttitudeTransform>
#include <osg/MatrixTransform>
#include <osg/LineWidth>
#include <osg/Program>
#include <osg/Shader>
#include <map>

class OdomCameraVisualizer {
public:
    OdomCameraVisualizer() {
        root_group_ = new osg::Group();
        history_group_ = new osg::Group(); // 专门存放历史轨迹的组
        root_group_->addChild(history_group_);

        // 1. 创建历史轨迹的模具 (淡蓝色面，细黄线)
        history_model_ = createCameraNode(
            osg::Vec4(0.0f, 0.2f, 0.5f, 0.3f),  // 面的颜色 (低透明度)
            osg::Vec4(1.0f, 1.0f, 1.0f, 0.3f),  // 线的颜色
            0.5f                                // 线宽
        );

        // 2. 创建当前高亮相机的模具 (亮红色面，粗红线)
        current_model_ = createCameraNode(
            osg::Vec4(1.0f, 0.0f, 0.0f, 0.8f),  // 面的颜色 (偏红)
            osg::Vec4(1.0f, 0.0f, 0.0f, 1.0f),  // 线的颜色 (纯红)
            2.0f                                // 线宽粗一点
        );

        // 3. 初始化当前相机节点 (全局只有一个)
        current_pose_pat_ = new osg::PositionAttitudeTransform();
        current_pose_pat_->addChild(current_model_);
        current_pose_pat_->setNodeMask(0); // 初始隐藏，直到收到第一帧数据
        root_group_->addChild(current_pose_pat_);
    }

    ~OdomCameraVisualizer() {}

    osg::ref_ptr<osg::Group> getRootNode() { return root_group_; }

    // 注意：这里增加了一个 index 参数！
    void updatePose(int index, double x, double y, double z,
        double qx, double qy, double qz, double qw) {
        if (!root_group_) return;

        osg::Vec3d pos(x, y, z);
        osg::Quat quat(qx, qy, qz, qw);

        // 1. 移动当前高亮相机
        current_pose_pat_->setNodeMask(0xffffffff); // 显示相机
        current_pose_pat_->setPosition(pos);
        current_pose_pat_->setAttitude(quat);

        // 2. 处理倒退 (Rewind)
        // 如果数据库发来了上一帧，把比当前 index 大的所有未来轨迹删掉
        auto it = pose_trail_.upper_bound(index);
        while (it != pose_trail_.end()) {
            history_group_->removeChild(it->second); // 从场景移除
            it = pose_trail_.erase(it);              // 从 map 移除
        }

        // 3. 去重与新增历史轨迹
        // 如果这个 index 是新的，才把它加入历史轨迹
        if (pose_trail_.find(index) == pose_trail_.end()) {
            osg::ref_ptr<osg::PositionAttitudeTransform> pat = new osg::PositionAttitudeTransform();
            pat->setPosition(pos);
            pat->setAttitude(quat);
            pat->addChild(history_model_);

            history_group_->addChild(pat);
            pose_trail_[index] = pat;
        }
    }

    void clear() {
        pose_trail_.clear();
        if (history_group_) {
            history_group_->removeChildren(0, history_group_->getNumChildren());
        }
        if (current_pose_pat_) {
            current_pose_pat_->setNodeMask(0); // 重新隐藏当前相机
        }
    }

private:
    osg::ref_ptr<osg::Group> root_group_;
    osg::ref_ptr<osg::Group> history_group_;
    osg::ref_ptr<osg::PositionAttitudeTransform> current_pose_pat_;

    // 两个模具 (节点共享)
    osg::ref_ptr<osg::Node> history_model_;
    osg::ref_ptr<osg::Node> current_model_;

    // 用 map 追踪所有已生成的位姿，Key 是帧的 index
    std::map<int, osg::ref_ptr<osg::PositionAttitudeTransform>> pose_trail_;

    // 辅助工厂函数：根据指定颜色和线宽生成视锥体模具
    osg::ref_ptr<osg::Node> createCameraNode(const osg::Vec4& face_color, const osg::Vec4& line_color, float line_width_val) {
        osg::ref_ptr<osg::MatrixTransform> local_rot = new osg::MatrixTransform();
        osg::ref_ptr<osg::Geode> geode = new osg::Geode();

        float half = 0.2f;
        float height = 0.5f;
        osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array;
        vertices->push_back(osg::Vec3(-half, -half, height)); // 0
        vertices->push_back(osg::Vec3(half, -half, height));  // 1
        vertices->push_back(osg::Vec3(half, half, height));   // 2
        vertices->push_back(osg::Vec3(-half, half, height));  // 3
        vertices->push_back(osg::Vec3(0, 0, 0));              // 4 (光心)

        // --- 绘制面 ---
        osg::ref_ptr<osg::Geometry> face_geometry = new osg::Geometry();
        face_geometry->setVertexArray(vertices);
        osg::ref_ptr<osg::DrawElementsUInt> face_indices = new osg::DrawElementsUInt(osg::PrimitiveSet::TRIANGLES);
        face_indices->push_back(0); face_indices->push_back(1); face_indices->push_back(2);
        face_indices->push_back(0); face_indices->push_back(2); face_indices->push_back(3);
        face_indices->push_back(0); face_indices->push_back(1); face_indices->push_back(4);
        face_indices->push_back(1); face_indices->push_back(2); face_indices->push_back(4);
        face_indices->push_back(2); face_indices->push_back(3); face_indices->push_back(4);
        face_indices->push_back(3); face_indices->push_back(0); face_indices->push_back(4);
        face_geometry->addPrimitiveSet(face_indices);

        osg::ref_ptr<osg::Vec4Array> face_colors = new osg::Vec4Array;
        for (int i = 0; i < 5; ++i) face_colors->push_back(face_color);
        face_geometry->setColorArray(face_colors, osg::Array::BIND_PER_VERTEX);
        face_geometry->getOrCreateStateSet()->setMode(GL_BLEND, osg::StateAttribute::ON);
        face_geometry->getOrCreateStateSet()->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
        geode->addDrawable(face_geometry);

        // --- 绘制线框 ---
        osg::ref_ptr<osg::Geometry> line_geometry = new osg::Geometry();
        line_geometry->setVertexArray(vertices);
        osg::ref_ptr<osg::DrawElementsUInt> indices = new osg::DrawElementsUInt(osg::PrimitiveSet::LINES);
        indices->push_back(0); indices->push_back(1); indices->push_back(1); indices->push_back(2);
        indices->push_back(2); indices->push_back(3); indices->push_back(3); indices->push_back(0);
        indices->push_back(0); indices->push_back(4); indices->push_back(1); indices->push_back(4);
        indices->push_back(2); indices->push_back(4); indices->push_back(3); indices->push_back(4);
        line_geometry->addPrimitiveSet(indices);

        osg::ref_ptr<osg::Vec4Array> line_colors = new osg::Vec4Array;
        for (int i = 0; i < 5; ++i) line_colors->push_back(line_color);
        line_geometry->setColorArray(line_colors, osg::Array::BIND_PER_VERTEX);
        osg::ref_ptr<osg::LineWidth> line_width = new osg::LineWidth(line_width_val);
        line_geometry->getOrCreateStateSet()->setAttributeAndModes(line_width, osg::StateAttribute::ON);
        geode->addDrawable(line_geometry);

        // --- 绘制坐标轴 ---
        osg::ref_ptr<osg::Geometry> axes_geom = new osg::Geometry();
        osg::ref_ptr<osg::Vec3Array> axes_verts = new osg::Vec3Array;
        osg::ref_ptr<osg::Vec4Array> axes_cols = new osg::Vec4Array;
        float ax_len = 0.2f;
        axes_verts->push_back(osg::Vec3(0, 0, 0)); axes_verts->push_back(osg::Vec3(ax_len, 0, 0));
        axes_cols->push_back(osg::Vec4(1, 0, 0, 1)); axes_cols->push_back(osg::Vec4(1, 0, 0, 1));
        axes_verts->push_back(osg::Vec3(0, 0, 0)); axes_verts->push_back(osg::Vec3(0, ax_len, 0));
        axes_cols->push_back(osg::Vec4(0, 1, 0, 1)); axes_cols->push_back(osg::Vec4(0, 1, 0, 1));
        axes_verts->push_back(osg::Vec3(0, 0, 0)); axes_verts->push_back(osg::Vec3(0, 0, ax_len));
        axes_cols->push_back(osg::Vec4(0, 0, 1, 1)); axes_cols->push_back(osg::Vec4(0, 0, 1, 1));
        axes_geom->setVertexArray(axes_verts);
        axes_geom->setColorArray(axes_cols, osg::Array::BIND_PER_VERTEX);
        axes_geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::LINES, 0, 6));
        osg::ref_ptr<osg::LineWidth> ax_lw = new osg::LineWidth(line_width_val + 1.0f);
        axes_geom->getOrCreateStateSet()->setAttributeAndModes(ax_lw, osg::StateAttribute::ON);
        geode->addDrawable(axes_geom);

        local_rot->addChild(geode);

        // --- Shader 挂载 ---
        const char* vertSource =
            "uniform mat4 osg_ModelViewProjectionMatrix;\n"
            "attribute vec4 osg_Vertex;\n"
            "attribute vec4 osg_Color;\n"
            "varying vec4 v_color;\n"
            "void main() {\n"
            "   v_color = osg_Color;\n"
            "   gl_Position = osg_ModelViewProjectionMatrix * osg_Vertex;\n"
            "}\n";
        const char* fragSource =
            "varying vec4 v_color;\n"
            "void main() {\n"
            "   gl_FragColor = v_color;\n"
            "}\n";
        osg::ref_ptr<osg::Program> program = new osg::Program;
        program->addShader(new osg::Shader(osg::Shader::VERTEX, vertSource));
        program->addShader(new osg::Shader(osg::Shader::FRAGMENT, fragSource));
        local_rot->getOrCreateStateSet()->setAttributeAndModes(program, osg::StateAttribute::ON);

        return local_rot;
    }
};