#ifndef ODOMPATHVISUALIZER_H
#define ODOMPATHVISUALIZER_H

#include <osg/Group>
#include <osg/Geode>
#include <osg/Point>
#include <osg/LineWidth>

class OdomPathVisualizer {
public:
	OdomPathVisualizer() {
		_pathGeode = new osg::Geode();
		osg::ref_ptr<osg::Geometry> geom = new osg::Geometry();

		geom->setDataVariance(osg::Object::DYNAMIC);
		geom->setUseDisplayList(false);
		geom->setUseVertexBufferObjects(true);

		vertices = new osg::Vec3Array();
		colors = new osg::Vec4Array();
		colors->push_back(osg::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
		geom->setColorArray(colors, osg::Array::BIND_OVERALL);
		geom->setVertexArray(vertices);
		geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::LINE_STRIP, 0, 0));
		geom->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);

		geom->getOrCreateStateSet()->setAttributeAndModes(new osg::LineWidth(2.0f), osg::StateAttribute::ON);
		_pathGeode->addDrawable(geom);
	}
	~OdomPathVisualizer() {}

	osg::ref_ptr<osg::Geode> getNode() { return _pathGeode; }

	void updatePose(double x, double y, double z) {
		qDebug() << "Path Point:" << x << y << z;
		if (!_pathGeode) {
			_pathGeode = new osg::Geode();
			osg::ref_ptr<osg::Geometry> geom = new osg::Geometry();
			vertices = new osg::Vec3Array();
			colors = new osg::Vec4Array();
			colors->push_back(osg::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
			geom->setColorArray(colors, osg::Array::BIND_OVERALL);
			geom->setVertexArray(vertices);
			geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::LINE_STRIP, 0, 0));
			_pathGeode->addDrawable(geom);
		}

		osg::Geometry* geom = dynamic_cast<osg::Geometry*>(_pathGeode->getDrawable(0));
		if (!geom) return;
		vertices->push_back(osg::Vec3(x, y, z));
		vertices->dirty(); // 显式通知数组已更改

		// 2. 更新绘制计数 (关键！)
		osg::DrawArrays* da = dynamic_cast<osg::DrawArrays*>(geom->getPrimitiveSet(0));
		if (da) {
			da->setCount(vertices->size());
			da->dirty(); // 通知绘制数组已更改
		}

		// 3. 更新包围球，防止被剔除
		geom->dirtyBound();
	}
private:
	osg::ref_ptr<osg::Geode> _pathGeode;
	osg::ref_ptr<osg::Vec3Array> vertices;
	osg::ref_ptr<osg::Vec4Array> colors;
};
#endif