#ifndef LIVOXCLOUDVISUALIZER_H
#define LIVOXCLOUDVISUALIZER_H


#include <osg/Group>
#include <osg/Geometry>
#include <osg/Geode>
#include <osg/Point>
#include <osg/BlendFunc>
#include <osg/BlendColor>

class LivoxCloudVisualizer {
public:
	LivoxCloudVisualizer() {
		_cloudGeode = new osg::Geode();
		osg::ref_ptr<osg::Geometry> geom = new osg::Geometry();

		geom->setDataVariance(osg::Object::DYNAMIC);
		geom->setUseDisplayList(false);
		geom->setUseVertexBufferObjects(true);

		geom->setVertexArray(new osg::Vec3Array());
		geom->setColorArray(new osg::Vec4Array(), osg::Array::BIND_PER_VERTEX);
		geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::POINTS, 0, 0));
		_cloudGeode->addDrawable(geom);
		applyRenderState(geom);
	}
	~LivoxCloudVisualizer() {};

	osg::ref_ptr<osg::Geode> getNode() { return _cloudGeode; }

	void updateCloud(const std::vector<GeneralPointIRGB>& points) {
		if (!_cloudGeode) {
			_cloudGeode = new osg::Geode();
			osg::ref_ptr<osg::Geometry> geom = new osg::Geometry();

			geom->setDataVariance(osg::Object::DYNAMIC);
			geom->setUseDisplayList(false);
			geom->setUseVertexBufferObjects(true);

			geom->setVertexArray(new osg::Vec3Array());
			geom->setColorArray(new osg::Vec4Array(), osg::Array::BIND_PER_VERTEX);
			geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::POINTS, 0, 0));
			_cloudGeode->addDrawable(geom);
			applyRenderState(geom);
		}

		osg::Geometry* geom = dynamic_cast<osg::Geometry*>(_cloudGeode->getDrawable(0));
		if (!geom) return;

		osg::Vec3Array* vertices = dynamic_cast<osg::Vec3Array*>(geom->getVertexArray());
		osg::Vec4Array* colors = dynamic_cast<osg::Vec4Array*>(geom->getColorArray());

		if (!vertices || !colors) return;

		vertices->clear();
		colors->clear();

		for (const auto& pt : points) {
			vertices->push_back(osg::Vec3(pt.pointI.x, pt.pointI.y, pt.pointI.z));
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
		m_currentPointSize = static_cast<float>(size);
		if (_cloudGeode && _cloudGeode->getNumDrawables() > 0) {
			applyRenderState(dynamic_cast<osg::Geometry*>(_cloudGeode->getDrawable(0)));
		}
	}
	void updateOpacity(const int opacity) {
		m_currentOpacity = opacity;
		if (_cloudGeode && _cloudGeode->getNumDrawables() > 0) {
			applyRenderState(dynamic_cast<osg::Geometry*>(_cloudGeode->getDrawable(0)));
		}
	}
private:
	void applyRenderState(osg::Geometry* geom) {
		if (!geom) return;
		osg::StateSet* state = geom->getOrCreateStateSet();

		// 应用保存的点大小
		state->setAttributeAndModes(new osg::Point(m_currentPointSize), osg::StateAttribute::ON);

		// 应用保存的透明度
		float alpha = static_cast<float>(m_currentOpacity) / 100.0f;
		if (alpha < 1.0f) {
			osg::ref_ptr<osg::BlendColor> bc = new osg::BlendColor(osg::Vec4(1.0, 1.0, 1.0, alpha));
			state->setAttributeAndModes(bc, osg::StateAttribute::ON);
			state->setAttributeAndModes(new osg::BlendFunc(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA), osg::StateAttribute::ON);
			state->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
		}
		else {
			state->removeAttribute(osg::StateAttribute::BLENDCOLOR);
			state->setRenderingHint(osg::StateSet::DEFAULT_BIN);
			state->setAttributeAndModes(new osg::BlendFunc(), osg::StateAttribute::OFF);
		}
		state->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
	}

	osg::Geode* _cloudGeode;
	int m_currentPointSize = 2;
	int m_currentOpacity = 100;

};
#endif 