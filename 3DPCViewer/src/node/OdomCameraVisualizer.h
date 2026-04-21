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

class OdomCameraVisualizer {
 public:
     OdomCameraVisualizer() {
         root_group_ = new osg::Group();
         local_model_rotation_ = new osg::MatrixTransform();

         osg::ref_ptr<osg::Geode> geode = new osg::Geode();

         // ==================== 1. 共享顶点 ====================
         float half = 0.2f;
         float height = 0.5f;
         osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array;
         vertices->push_back(osg::Vec3(-half, -half, height)); // 0
         vertices->push_back(osg::Vec3(half, -half, height));  // 1
         vertices->push_back(osg::Vec3(half, half, height));   // 2
         vertices->push_back(osg::Vec3(-half, half, height));  // 3
         vertices->push_back(osg::Vec3(0, 0, 0));              // 4 (光心)

         // ==================== 2. 绘制面 (半透明蓝) ====================
         osg::ref_ptr<osg::Geometry> face_geometry = new osg::Geometry();
         face_geometry->setVertexArray(vertices);

         osg::ref_ptr<osg::DrawElementsUInt> face_indices = new osg::DrawElementsUInt(osg::PrimitiveSet::TRIANGLES);
         // 底面
         face_indices->push_back(0); face_indices->push_back(1); face_indices->push_back(2);
         face_indices->push_back(0); face_indices->push_back(2); face_indices->push_back(3);
         /*
         face_indices->push_back(0); face_indices->push_back(1); face_indices->push_back(4);
         face_indices->push_back(1); face_indices->push_back(2); face_indices->push_back(4);
         face_indices->push_back(2); face_indices->push_back(3); face_indices->push_back(4);
         face_indices->push_back(3); face_indices->push_back(0); face_indices->push_back(4);
         */
         face_geometry->addPrimitiveSet(face_indices);
         
         // 【修改点】：使用 BIND_PER_VERTEX，为5个顶点分别塞入同一个蓝色
         osg::ref_ptr<osg::Vec4Array> face_colors = new osg::Vec4Array;
         for (int i = 0; i < 5; ++i) {
             face_colors->push_back(osg::Vec4(0.0f, 0.5f, 1.0f, 0.4f));
         }
         face_geometry->setColorArray(face_colors, osg::Array::BIND_PER_VERTEX);

         // 开启混合与透明度
         face_geometry->getOrCreateStateSet()->setMode(GL_BLEND, osg::StateAttribute::ON);
         face_geometry->getOrCreateStateSet()->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);

         geode->addDrawable(face_geometry);

         // ==================== 3. 绘制线框 (黄色) ====================
         osg::ref_ptr<osg::Geometry> line_geometry = new osg::Geometry();
         line_geometry->setVertexArray(vertices);

         osg::ref_ptr<osg::DrawElementsUInt> indices = new osg::DrawElementsUInt(osg::PrimitiveSet::LINES);
         indices->push_back(0); indices->push_back(1);
         indices->push_back(1); indices->push_back(2);
         indices->push_back(2); indices->push_back(3);
         indices->push_back(3); indices->push_back(0);
         indices->push_back(0); indices->push_back(4);
         indices->push_back(1); indices->push_back(4);
         indices->push_back(2); indices->push_back(4);
         indices->push_back(3); indices->push_back(4);
         line_geometry->addPrimitiveSet(indices);

         // 【修改点】：使用 BIND_PER_VERTEX，为5个顶点分别塞入同一个黄色
         osg::ref_ptr<osg::Vec4Array> line_colors = new osg::Vec4Array;
         for (int i = 0; i < 5; ++i) {
             line_colors->push_back(osg::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
         }
         line_geometry->setColorArray(line_colors, osg::Array::BIND_PER_VERTEX);

         osg::ref_ptr<osg::LineWidth> line_width = new osg::LineWidth(2.0f);
         line_geometry->getOrCreateStateSet()->setAttributeAndModes(line_width, osg::StateAttribute::ON);

         geode->addDrawable(line_geometry);
         local_model_rotation_->addChild(geode);

         // ==================== 4. 挂载 Shader (修复了变量声明) ====================
         // 【修改点】：加上了 attribute 和 uniform 的声明！
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

         root_group_->getOrCreateStateSet()->setAttributeAndModes(program, osg::StateAttribute::ON);
     }

  ~OdomCameraVisualizer() {}

  osg::ref_ptr<osg::Group> getRootNode() { return root_group_; }

  void updatePose(double x, double y, double z,
                  double qx, double qy, double qz, double qw) {
    if (!root_group_) return;

    osg::ref_ptr<osg::PositionAttitudeTransform> pose_transform = new osg::PositionAttitudeTransform();
    pose_transform->setPosition(osg::Vec3d(x, y, z));

    osg::Quat quat(qx, qy, qz, qw);
    pose_transform->setAttitude(quat);

    pose_transform->addChild(local_model_rotation_);
    root_group_->addChild(pose_transform);
  }

  void clear() {
    if (root_group_) {
      root_group_->removeChildren(0, root_group_->getNumChildren());
    }
  }

 private:
  osg::ref_ptr<osg::Group> root_group_;
  osg::ref_ptr<osg::MatrixTransform> local_model_rotation_;
};
