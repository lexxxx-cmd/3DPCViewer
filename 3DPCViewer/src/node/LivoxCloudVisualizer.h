#pragma once

#include <osg/Group>
#include <osg/Geometry>
#include <osg/Geode>
#include <osg/Point>
#include <osg/BlendFunc>
#include <osg/BlendColor>

class LivoxCloudVisualizer {
 public:
  LivoxCloudVisualizer() {
    cloud_geode_ = new osg::Geode();
    osg::ref_ptr<osg::Geometry> geom = new osg::Geometry();

    geom->setDataVariance(osg::Object::DYNAMIC);
    geom->setUseDisplayList(false);
    geom->setUseVertexBufferObjects(true);

    geom->setVertexArray(new osg::Vec3Array());
    geom->setColorArray(new osg::Vec4Array(), osg::Array::BIND_PER_VERTEX);
    geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::POINTS, 0, 0));
    cloud_geode_->addDrawable(geom);

    point_attribute_ = new osg::Point(2.0f);
    geom->getOrCreateStateSet()->setAttributeAndModes(point_attribute_, osg::StateAttribute::ON);
    applyRenderState(geom);
  }
  ~LivoxCloudVisualizer() {};

  osg::ref_ptr<osg::Geode> getNode() { return cloud_geode_; }

  void updateCloud(const std::vector<GeneralPointIRGB>& points) {
    if (!cloud_geode_) {
      cloud_geode_ = new osg::Geode();
      osg::ref_ptr<osg::Geometry> geom = new osg::Geometry();

      geom->setDataVariance(osg::Object::DYNAMIC);
      geom->setUseDisplayList(false);
      geom->setUseVertexBufferObjects(true);

      geom->setVertexArray(new osg::Vec3Array());
      geom->setColorArray(new osg::Vec4Array(), osg::Array::BIND_PER_VERTEX);
      geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::POINTS, 0, 0));
      cloud_geode_->addDrawable(geom);
      applyRenderState(geom);
    }

    osg::Geometry* geom = dynamic_cast<osg::Geometry*>(cloud_geode_->getDrawable(0));
    if (!geom) return;

    osg::Vec3Array* vertices = dynamic_cast<osg::Vec3Array*>(geom->getVertexArray());
    osg::Vec4Array* colors = dynamic_cast<osg::Vec4Array*>(geom->getColorArray());

    if (!vertices || !colors) return;

    vertices->clear();
    colors->clear();

    for (const auto& pt : points) {
      vertices->push_back(osg::Vec3(pt.point_i.x, pt.point_i.y, pt.point_i.z));
      colors->push_back(osg::Vec4(
          pt.r / 255.0f,
          pt.g / 255.0f,
          pt.b / 255.0f,
          1.0f
      ));
    }
    vertices->dirty();
    colors->dirty();
    geom->setVertexArray(vertices);
    geom->setColorArray(colors, osg::Array::BIND_PER_VERTEX);

    applyRenderState(geom);

    osg::DrawArrays* da = dynamic_cast<osg::DrawArrays*>(geom->getPrimitiveSet(0));
    if (da) {
      da->setCount(vertices->size());
      da->dirty();
    }

    geom->dirtyDisplayList();
    geom->dirtyBound();
  }

  void updatePointSize(const int size) {
    current_point_size_ = static_cast<float>(size);
    if (point_attribute_.valid()) {
      point_attribute_->setSize(current_point_size_);
    }
    if (cloud_geode_ && cloud_geode_->getNumDrawables() > 0) {
      applyRenderState(dynamic_cast<osg::Geometry*>(cloud_geode_->getDrawable(0)));
    }
  }

  void updateOpacity(const int opacity) {
    current_opacity_ = opacity;
    if (cloud_geode_ && cloud_geode_->getNumDrawables() > 0) {
      applyRenderState(dynamic_cast<osg::Geometry*>(cloud_geode_->getDrawable(0)));
    }
  }

 private:
  void applyRenderState(osg::Geometry* geom) {
    if (!geom) return;
    osg::StateSet* state = geom->getOrCreateStateSet();

    state->setAttributeAndModes(new osg::Point(current_point_size_), osg::StateAttribute::ON);

    float alpha = static_cast<float>(current_opacity_) / 100.0f;
    if (alpha < 1.0f) {
      osg::ref_ptr<osg::BlendColor> bc = new osg::BlendColor(osg::Vec4(1.0, 1.0, 1.0, alpha));
      state->setAttributeAndModes(bc, osg::StateAttribute::ON);
      state->setAttributeAndModes(new osg::BlendFunc(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA), osg::StateAttribute::ON);
      state->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
    } else {
      state->removeAttribute(osg::StateAttribute::BLENDCOLOR);
      state->setRenderingHint(osg::StateSet::DEFAULT_BIN);
      state->setAttributeAndModes(new osg::BlendFunc(), osg::StateAttribute::OFF);
    }
    state->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
  }

  osg::Geode* cloud_geode_;
  osg::ref_ptr<osg::Point> point_attribute_;

  int current_point_size_ = 2;
  int current_opacity_ = 100;
};
