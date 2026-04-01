#include "visualareawidget.h"
#include "ui_visualareawidget.h"
#include "DataLoader.h"
#include <QtConcurrent/QtConcurrent>
#include <osg/StateSet>
#include <osg/BlendFunc>
#include <osg/BlendColor>
#include <QVBoxLayout>
#include <QFileInfo>

VisualAreaWidget::VisualAreaWidget(QWidget* parent) : QWidget(parent), ui(new Ui::VisualAreaWidgetClass) {
    ui->setupUi(this);

    m_osgWidget = new osgQOpenGLWidget(this);
    ui->gridLayout_2->addWidget(m_osgWidget);

    connect(m_osgWidget, &osgQOpenGLWidget::initialized, this, &VisualAreaWidget::initOSG);
}

void VisualAreaWidget::initOSG() {
    //位姿节点
    _camViz = std::make_unique<OdomCameraVisualizer>();
    _pathViz = std::make_unique<OdomPathVisualizer>();
    _gridViz = std::make_unique<Grid>(100);

    m_root = new osg::Group();
    osgViewer::Viewer* viewer = m_osgWidget->getOsgViewer();

    m_root->addChild(_camViz->getRootNode());
    m_root->addChild(_pathViz->getNode());
    m_root->addChild(_gridViz->getGridNode());

    viewer->setSceneData(m_root);
    viewer->setCameraManipulator(new osgGA::TrackballManipulator);
    viewer->getCamera()->setClearColor(osg::Vec4(0.1, 0.1, 0.1, 1.0));
}




void VisualAreaWidget::updateCloudGeometry(osg::ref_ptr<osg::Geometry> geom, bool isFirstLoad) {
    m_cloudGeom = geom;

    if (m_cloudGeode.valid()) {
        m_root->removeChild(m_cloudGeode);
    }

    m_cloudGeode = new osg::Geode();
    m_cloudGeode->addDrawable(m_cloudGeom);

    // 重新应用之前的点大小和透明度
    onChangeSizeRequested(m_currentPointSize);
    onChangeOpacityRequested(m_currentOpacity);
    m_cloudGeode->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);

    m_root->addChild(m_cloudGeode);

    if (isFirstLoad) {
        m_osgWidget->getOsgViewer()->home();
    }
    m_osgWidget->update();
}

void VisualAreaWidget::onChangeSizeRequested(const int& size) {
    m_currentPointSize = size;
    if (m_cloudGeom.valid()) {
        m_cloudGeom->getOrCreateStateSet()->setAttribute(new osg::Point(static_cast<float>(size)), osg::StateAttribute::ON);
        m_osgWidget->update();
    }
}

void VisualAreaWidget::onChangeOpacityRequested(const int& opacity) {
    m_currentOpacity = opacity;
    if (!m_cloudGeom.valid()) return;

    float alpha = static_cast<float>(opacity) / 100.0f;
    osg::StateSet* state = m_cloudGeom->getOrCreateStateSet();

    if (alpha < 1.0f) {
        osg::ref_ptr<osg::BlendColor> bc = new osg::BlendColor(osg::Vec4(1.0, 1.0, 1.0, alpha));
        state->setAttributeAndModes(bc, osg::StateAttribute::ON);
        state->setAttributeAndModes(new osg::BlendFunc(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA), osg::StateAttribute::ON);
        state->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
    }
    else {
        state->removeAttribute(osg::StateAttribute::BLENDCOLOR);
        state->setAttributeAndModes(new osg::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA), osg::StateAttribute::OFF);
        state->setRenderingHint(osg::StateSet::DEFAULT_BIN);
    }
    m_osgWidget->update();
}


VisualAreaWidget::~VisualAreaWidget() {
    delete ui;
}

void VisualAreaWidget::onCloudFrameReady(const LivoxCloudFrame& frame) {
    osg::ref_ptr<osg::Geometry> geom = DataLoader::convertToOsgGeometry(frame);
    updateCloudGeometry(geom, true);
}

void VisualAreaWidget::onImageFrameReady() {
    //
}

void VisualAreaWidget::onOdomFrameReady(const OdomFrame& frame) {
    if (_camViz)
    {
        // 更新 OSG 节点
        _camViz->updatePose(frame.pose.x,frame.pose.y,frame.pose.z,
            frame.pose.qx,frame.pose.qy,frame.pose.qz,frame.pose.qw);
    }

    if (_pathViz)
    {
        _pathViz->updatePose(frame.pose.x, frame.pose.y, frame.pose.z);
    }
    m_osgWidget->update();
}