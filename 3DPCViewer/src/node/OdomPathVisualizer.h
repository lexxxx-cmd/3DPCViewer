#pragma once

#include <osg/Group>
#include <osg/Geode>
#include <osg/Point>
#include <osg/LineWidth>

class OdomPathVisualizer {
 public:
  OdomPathVisualizer() {
    path_geode_ = new osg::Geode();
    osg::ref_ptr<osg::Geometry> geom = new osg::Geometry();

    geom->setDataVariance(osg::Object::DYNAMIC);
    geom->setUseDisplayList(false);
    geom->setUseVertexBufferObjects(true);

    vertices_ = new osg::Vec3Array();
    colors_ = new osg::Vec4Array();
    colors_->push_back(osg::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
    geom->setColorArray(colors_, osg::Array::BIND_OVERALL);
    geom->setVertexArray(vertices_);
    geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::LINE_STRIP, 0, 0));
    geom->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);

    geom->getOrCreateStateSet()->setAttributeAndModes(new osg::LineWidth(2.0f), osg::StateAttribute::ON);
    path_geode_->addDrawable(geom);
  }
  ~OdomPathVisualizer() {}

  osg::ref_ptr<osg::Geode> getNode() { return path_geode_; }

  void updatePose(double x, double y, double z, int index) {
    if (!path_geode_) {
      path_geode_ = new osg::Geode();
      osg::ref_ptr<osg::Geometry> geom = new osg::Geometry();
      vertices_ = new osg::Vec3Array();
      colors_ = new osg::Vec4Array();
      colors_->push_back(osg::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
      geom->setColorArray(colors_, osg::Array::BIND_OVERALL);
      geom->setVertexArray(vertices_);
      geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::LINE_STRIP, 0, 0));
      path_geode_->addDrawable(geom);
    }

    osg::Geometry* geom = dynamic_cast<osg::Geometry*>(path_geode_->getDrawable(0));
    if (!geom) return;

    osg::DrawArrays* da = dynamic_cast<osg::DrawArrays*>(geom->getPrimitiveSet(0));
    if (da) {
      if (index >= max_odom_num_) {
        vertices_->push_back(osg::Vec3(x, y, z));
        max_odom_num_++;
        vertices_->dirty();
        da->setCount(vertices_->size());
        da->dirty();
      } else {
        da->setCount(index);
        da->dirty();
      }
    }

    geom->dirtyBound();
  }

 private:
  osg::ref_ptr<osg::Geode> path_geode_;
  osg::ref_ptr<osg::Vec3Array> vertices_;
  osg::ref_ptr<osg::Vec4Array> colors_;

  int max_odom_num_ = 0;
};
