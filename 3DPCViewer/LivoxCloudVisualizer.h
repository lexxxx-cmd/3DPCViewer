#ifndef LIVOXCLOUDVISUALIZER_H
#define LIVOXCLOUDVISUALIZER_H


#include <osg/Group>
#include <osg/Geometry>
#include <osg/Geode>
#include <osg/Point>

class LivoxCloudVisualizer {
public:
	LivoxCloudVisualizer() {
		_cloudGeode = new osg::Geode();
		osg::ref_ptr<osg::Geometry> geom = new osg::Geometry();

		geom->setDataVariance(osg::Object::DYNAMIC);
		geom->setUseDisplayList(false);
		geom->setUseVertexBufferObjects(true);

		osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();
		osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
		colors->push_back(osg::Vec4(0.0f, 1.0f, 0.0f, 1.0f));
		geom->setColorArray(colors, osg::Array::BIND_OVERALL);
		geom->setVertexArray(vertices);

		geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::POINTS, 0, 0));
		geom->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);

		geom->getOrCreateStateSet()->setAttributeAndModes(new osg::Point(2.0f), osg::StateAttribute::ON);
		_cloudGeode->addDrawable(geom);
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

			osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();
			osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
			colors->push_back(osg::Vec4(0.0f, 1.0f, 0.0f, 1.0f));
			geom->setColorArray(colors, osg::Array::BIND_OVERALL);
			geom->setVertexArray(vertices);

			geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::POINTS, 0, 0));
			geom->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);

			geom->getOrCreateStateSet()->setAttributeAndModes(new osg::Point(2.0f), osg::StateAttribute::ON);
			_cloudGeode->addDrawable(geom);
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

		geom->setVertexArray(vertices);
		geom->setColorArray(colors, osg::Array::BIND_PER_VERTEX);
		osg::DrawArrays* da = dynamic_cast<osg::DrawArrays*>(geom->getPrimitiveSet(0));
		if (da) {
			da->setCount(vertices->size());
			da->dirty();
		}

		geom->dirtyDisplayList();
		geom->dirtyBound();
	}
	void updatePointSize(const int size) {}
	void updateOpacity(const int opacity) {}
private:
	osg::Geode* _cloudGeode;

};
#endif 