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

    connect(&m_loadWatcher, &QFutureWatcher<pcl::PCLPointCloud2::Ptr>::finished, this, &VisualAreaWidget::onDataLoaded);
    connect(&m_downsampleWatcher, &QFutureWatcher<pcl::PCLPointCloud2::Ptr>::finished, this, &VisualAreaWidget::onDataDownsampled);
    connect(&m_normalWatcher, &QFutureWatcher<osg::ref_ptr<osg::Geometry>>::finished, this, &VisualAreaWidget::onNormalsComputed);
}

void VisualAreaWidget::initOSG() {
    m_root = new osg::Group();
    osgViewer::Viewer* viewer = m_osgWidget->getOsgViewer();
    viewer->setSceneData(m_root);
    viewer->setCameraManipulator(new osgGA::TrackballManipulator);
    viewer->getCamera()->setClearColor(osg::Vec4(0.1, 0.1, 0.1, 1.0));
}

void VisualAreaWidget::onOpenFileRequested(const QString& path) {
    if (m_loadWatcher.isRunning() || m_downsampleWatcher.isRunning() || m_normalWatcher.isRunning()) return;

    QFileInfo info(path);
    emit sendFileSize(info.size() / 1024 / 1024);

    QFuture<pcl::PCLPointCloud2::Ptr> future = QtConcurrent::run(DataLoader::loadGenericPointCloud, path);
    m_loadWatcher.setFuture(future);
}

void VisualAreaWidget::onDataLoaded() {
    m_currentCloud = m_loadWatcher.result();
    if (!m_currentCloud || m_currentCloud->data.empty()) return;

    // 点云转OSG格式并显示
    osg::ref_ptr<osg::Geometry> geom = DataLoader::convertToOsgGeometry(m_currentCloud);
    updateCloudGeometry(geom, true);

    emit sendPointSize(m_currentCloud->width * m_currentCloud->height);

    // 采样
    adaptiveDownsampleAndComputeNormals();
}

void VisualAreaWidget::adaptiveDownsampleAndComputeNormals() {
    int pointCount = m_currentCloud->width * m_currentCloud->height;
    const int targetCount = 5000000;

    if (pointCount <= targetCount) {
        // 点数较少，无需降采样，直接计算法线
        QFuture<osg::ref_ptr<osg::Geometry>> future = QtConcurrent::run(DataLoader::computeNormalsAndConvert, m_currentCloud);
        m_normalWatcher.setFuture(future);
    }
    else {
        // 计算合适的leaf size
        float ratio = static_cast<float>(pointCount) / targetCount;
        m_leafSize = 0.01f * std::cbrt(ratio); // 立方根缩放leaf size
        QFuture<pcl::PCLPointCloud2::Ptr> future = QtConcurrent::run(DataLoader::downsampleCloud, m_currentCloud, m_leafSize);
        m_downsampleWatcher.setFuture(future);
    }
}

void VisualAreaWidget::onDataDownsampled() {
    m_currentCloud = m_downsampleWatcher.result();
    if (!m_currentCloud || m_currentCloud->data.empty()) return;

    // 更新
    osg::ref_ptr<osg::Geometry> geom = DataLoader::convertToOsgGeometry(m_currentCloud);
    updateCloudGeometry(geom, false);

    emit sendPointSize(m_currentCloud->width * m_currentCloud->height); // 更新降采样后的点数

    // 法线
    QFuture<osg::ref_ptr<osg::Geometry>> future = QtConcurrent::run(DataLoader::computeNormalsAndConvert, m_currentCloud);
    m_normalWatcher.setFuture(future);
}

void VisualAreaWidget::onNormalsComputed() {
    m_cloudNormalGeom = m_normalWatcher.result();

    if (m_normalGeode.valid()) {
        m_root->removeChild(m_normalGeode);
    }

    m_normalGeode = new osg::Geode();
    m_normalGeode->addDrawable(m_cloudNormalGeom);

    m_normalGeode->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);

    m_normalGeode->setNodeMask(m_showNormals ? 0xffffffff : 0x0);

    m_root->addChild(m_normalGeode);
    m_osgWidget->update();
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

void VisualAreaWidget::onShowNormalsRequested(const bool& show) {
    m_showNormals = show;
    if (m_normalGeode.valid()) {
        m_normalGeode->setNodeMask(show ? 0xffffffff : 0x0);
        m_osgWidget->update();
    }
}

VisualAreaWidget::~VisualAreaWidget() {
    delete ui;
}

void VisualAreaWidget::onCloudFrameReady(const LivoxCloudFrame& frame) {
    osg::ref_ptr<osg::Geometry> geom = DataLoader::convertToOsgGeometry(frame);
    updateCloudGeometry(geom, true);
    std::cout << "Received cloud frame with " << frame.points.size() << " points.";
    emit sendPointSize(frame.points.size());

}