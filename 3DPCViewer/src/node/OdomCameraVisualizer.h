#pragma once

#include <osg/Group>
#include <osg/Geode>
#include <osg/ShapeDrawable>
#include <osg/Material>
#include <osg/PositionAttitudeTransform>
#include <osg/MatrixTransform>
#include <osg/LineWidth>

class OdomCameraVisualizer {
 public:
  OdomCameraVisualizer() {
    root_group_ = new osg::Group();

    local_model_rotation_ = new osg::MatrixTransform();
    //local_model_rotation_->setMatrix(osg::Matrix::rotate(osg::DegreesToRadians(90.0), osg::Vec3(0, 1, 0)));

    osg::ref_ptr<osg::Geode> geode = new osg::Geode();
    osg::ref_ptr<osg::Geometry> pyramid = new osg::Geometry();

    float half = 0.2f;
    float height = 0.5f;
    osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array;
    vertices->push_back(osg::Vec3(-half, -half, height));
    vertices->push_back(osg::Vec3(half, -half, height));
    vertices->push_back(osg::Vec3(half, half, height));
    vertices->push_back(osg::Vec3(-half, half, height));
    vertices->push_back(osg::Vec3(0, 0, 0));
    pyramid->setVertexArray(vertices);

    osg::ref_ptr<osg::DrawElementsUInt> indices = new osg::DrawElementsUInt(osg::PrimitiveSet::LINES);
    indices->push_back(0); indices->push_back(1);
    indices->push_back(1); indices->push_back(2);
    indices->push_back(2); indices->push_back(3);
    indices->push_back(3); indices->push_back(0);
    indices->push_back(0); indices->push_back(4);
    indices->push_back(1); indices->push_back(4);
    indices->push_back(2); indices->push_back(4);
    indices->push_back(3); indices->push_back(4);

    pyramid->addPrimitiveSet(indices);

    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array;
    colors->push_back(osg::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
    pyramid->setColorArray(colors, osg::Array::BIND_OVERALL);

    osg::ref_ptr<osg::LineWidth> line_width = new osg::LineWidth(2.0f);
    pyramid->getOrCreateStateSet()->setAttributeAndModes(line_width, osg::StateAttribute::ON);
    pyramid->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);

    geode->addDrawable(pyramid);

    local_model_rotation_->addChild(geode);
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
