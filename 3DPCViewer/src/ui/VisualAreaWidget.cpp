#include "ui/VisualAreaWidget.h"
#include "ui_VisualAreaWidget.h"
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

    m_imagePanel = new ImagePanel(this);
    m_imagePanel->move(20, 20);
    m_imagePanel->raise();
    //m_imagePanel->setImage(QPixmap("E:\\C++_pj\\repos\\3DPCViewer\\data\\1774599812.5.jpg"));
}

void VisualAreaWidget::initOSG() {
    //位姿节点
    _camViz = std::make_unique<OdomCameraVisualizer>();
    _pathViz = std::make_unique<OdomPathVisualizer>();
    _gridViz = std::make_unique<Grid>(100);
    _livoxViz = std::make_unique<LivoxCloudVisualizer>();

    m_root = new osg::Group();
    osgViewer::Viewer* viewer = m_osgWidget->getOsgViewer();

    m_root->addChild(_camViz->getRootNode());
    m_root->addChild(_pathViz->getNode());
    m_root->addChild(_gridViz->getGridNode());
    m_root->addChild(_livoxViz->getNode());

    viewer->setSceneData(m_root);
    viewer->setCameraManipulator(new osgGA::TrackballManipulator);
    viewer->getCamera()->setClearColor(osg::Vec4(0.5, 0.7, 1.0, 1.0));
}




void VisualAreaWidget::updateCloudGeometry(osg::ref_ptr<osg::Geometry> geom, bool isFirstLoad) {
    m_cloudGeom = geom;

    if (m_cloudGeode.valid()) {
        m_root->removeChild(m_cloudGeode);
    }

    m_cloudGeode = new osg::Geode();
    m_cloudGeode->addDrawable(m_cloudGeom);

    // 重新应用之前的点大小和透明度
    

    m_root->addChild(m_cloudGeode);

    if (isFirstLoad) {
        m_osgWidget->getOsgViewer()->home();
    }
    m_osgWidget->update();
}

void VisualAreaWidget::onChangeSizeRequested(const int& size) {
    if (_livoxViz)
    {
        _livoxViz->updatePointSize(size);
    }
    m_osgWidget->update();
}

void VisualAreaWidget::onChangeOpacityRequested(const int& opacity) {
    if (_livoxViz)
    {
        _livoxViz->updateOpacity(opacity);
    }
    m_osgWidget->update();
}


VisualAreaWidget::~VisualAreaWidget() {
    delete ui;
    delete m_osgWidget;
    delete m_imagePanel;
    m_imagePanel = nullptr;
    m_osgWidget = nullptr;
}

void VisualAreaWidget::onCloudFrameReady(const GeneralCloudFrame& frame) {
    if (_livoxViz)
    {
        _livoxViz->updateCloud(frame.points);
    }
    m_osgWidget->update();
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
        _pathViz->updatePose(frame.pose.x, frame.pose.y, frame.pose.z, frame.index);
    }
    m_osgWidget->update();
}