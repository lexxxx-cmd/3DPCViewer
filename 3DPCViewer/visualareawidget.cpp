// 4. visualareawidget.cpp
#include "visualareawidget.h"
#include "ui_visualareawidget.h"
#include "DataLoader.h"
#include <QtConcurrent/QtConcurrent>
#include <osg/StateSet>
#include <osg/BlendFunc>
#include <osg/BlendColor>
#include <QVBoxLayout>

VisualAreaWidget::VisualAreaWidget(QWidget* parent) : QWidget(parent), ui(new Ui::VisualAreaWidgetClass) {
    ui->setupUi(this);

    // 初始化 OSG 部件
    m_osgWidget = new osgQOpenGLWidget(this);
    ui->gridLayout_2->addWidget(m_osgWidget);

    connect(m_osgWidget, &osgQOpenGLWidget::initialized, this, &VisualAreaWidget::initOSG);
    connect(&m_loadWatcher, &QFutureWatcher<CloudData>::finished, this, &VisualAreaWidget::onDataLoaded);
}

void VisualAreaWidget::initOSG() {
    m_root = new osg::Group();
    osgViewer::Viewer* viewer = m_osgWidget->getOsgViewer();
    viewer->setSceneData(m_root);
    viewer->setCameraManipulator(new osgGA::TrackballManipulator);
    viewer->getCamera()->setClearColor(osg::Vec4(0.1, 0.1, 0.1, 1.0));
}

void VisualAreaWidget::onOpenFileRequested(const QString& path) {
    if (m_loadWatcher.isRunning()) return;

    QFileInfo info(path);
    emit sendFileSize(info.size() / 1024 / 1024);

    // 异步调用统一加载接口
    QFuture<CloudData> future = QtConcurrent::run(DataLoader::loadFile, path);
    m_loadWatcher.setFuture(future);
}

void VisualAreaWidget::onDataLoaded() {
    CloudData data = m_loadWatcher.result();
    if (data.points.empty()) return;

    updateOsgScene(data);

    // 重置相机以观察全局
    m_osgWidget->getOsgViewer()->home();
    m_osgWidget->update();
    emit sendPointSize(data.points.size());
}

void VisualAreaWidget::updateOsgScene(const CloudData& data) {
    m_root->removeChildren(0, m_root->getNumChildren());

    m_cloudGeom = new osg::Geometry();
    osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array(data.points.begin(), data.points.end());
    m_cloudGeom->setVertexArray(vertices);

    if (data.hasColor) {
        osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array(data.colors.begin(), data.colors.end());
        m_cloudGeom->setColorArray(colors);
        m_cloudGeom->setColorBinding(osg::Geometry::BIND_PER_VERTEX);
       
    }
    else {
        osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
        colors->push_back(osg::Vec4(0.0, 1.0, 1.0, 1.0)); // 默认青色
        m_cloudGeom->setColorArray(colors);
        m_cloudGeom->setColorBinding(osg::Geometry::BIND_OVERALL);
    }

    m_cloudGeom->addPrimitiveSet(new osg::DrawArrays(GL_POINTS, 0, vertices->size()));

    osg::ref_ptr<osg::Geode> geode = new osg::Geode();
    geode->addDrawable(m_cloudGeom);

    // 设置默认点大小和禁用光照
    osg::StateSet* state = geode->getOrCreateStateSet();
    state->setAttribute(new osg::Point(2.0f), osg::StateAttribute::ON);
    state->setMode(GL_LIGHTING, osg::StateAttribute::OFF);

    m_root->addChild(geode);
}

void VisualAreaWidget::onChangeSizeRequested(const int& size) {
    if (m_cloudGeom.valid()) {
        m_cloudGeom->getOrCreateStateSet()->setAttribute(new osg::Point(static_cast<float>(size)), osg::StateAttribute::ON);
    }
}

// C++
void VisualAreaWidget::onChangeOpacityRequested(const int& opacity) {
    if (!m_cloudGeom.valid()) return;

    float alpha = static_cast<float>(opacity) / 100.0f;
    osg::StateSet* state = m_cloudGeom->getOrCreateStateSet();

    if (alpha < 1.0f) {
        // 1. 设置全局混合常量颜色
        osg::ref_ptr<osg::BlendColor> bc = new osg::BlendColor(osg::Vec4(1.0, 1.0, 1.0, alpha));
        state->setAttributeAndModes(bc, osg::StateAttribute::ON);

        // 2. 修改混合函数，使用常量 Alpha (GL_CONSTANT_ALPHA)
        state->setAttributeAndModes(new osg::BlendFunc(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA), osg::StateAttribute::ON);

        state->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
    }
    else {
        // 恢复默认
        state->removeAttribute(osg::StateAttribute::BLENDCOLOR);
        state->setAttributeAndModes(new osg::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA), osg::StateAttribute::OFF);
        state->setRenderingHint(osg::StateSet::DEFAULT_BIN);
    }
    m_osgWidget->update();
}

void VisualAreaWidget::onShowNormalsRequested(const bool& show) {
    // 逻辑同上，使用 osg::Geometry 绘制线条即可
    if (m_cloudNormalGeom.valid()) {
        m_cloudNormalGeom->setNodeMask(show ? 0xffffffff : 0x0);
        m_osgWidget->update();
    }
}

VisualAreaWidget::~VisualAreaWidget() { delete ui; }