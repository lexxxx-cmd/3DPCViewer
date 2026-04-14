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

  osg_widget = new osgQOpenGLWidget(this);
  ui->gridLayout_2->addWidget(osg_widget);

  connect(osg_widget, &osgQOpenGLWidget::initialized, this, &VisualAreaWidget::initOsg);
}

void VisualAreaWidget::initOsg() {
  cam_viz = std::make_unique<OdomCameraVisualizer>();
  path_viz = std::make_unique<OdomPathVisualizer>();
  grid_viz = std::make_unique<Grid>(100);
  livox_viz = std::make_unique<LivoxCloudVisualizer>();

  root = new osg::Group();
  osgViewer::Viewer* viewer = osg_widget->getOsgViewer();

  osg::State* state = viewer->getCamera()->getGraphicsContext()->getState();
  state->setUseModelViewAndProjectionUniforms(true);
  state->setUseVertexAttributeAliasing(true);

  root->addChild(cam_viz->getRootNode());
  root->addChild(path_viz->getNode());
  root->addChild(grid_viz->getGridNode());
  root->addChild(livox_viz->getNode());

  viewer->setSceneData(root);
  viewer->setCameraManipulator(new osgGA::TrackballManipulator);
  viewer->getCamera()->setClearColor(osg::Vec4(0.5, 0.7, 1.0, 1.0));
}

void VisualAreaWidget::updateCloudGeometry(osg::ref_ptr<osg::Geometry> geom, bool is_first_load) {
  cloud_geom = geom;

  if (cloud_geode.valid()) {
    root->removeChild(cloud_geode);
  }

  cloud_geode = new osg::Geode();
  cloud_geode->addDrawable(cloud_geom);

  root->addChild(cloud_geode);

  if (is_first_load) {
    osg_widget->getOsgViewer()->home();
  }
  osg_widget->update();
}

void VisualAreaWidget::onChangeSizeRequested(const int& size) {
  if (livox_viz) {
    livox_viz->updatePointSize(size);
  }
  osg_widget->update();
}

void VisualAreaWidget::onChangeOpacityRequested(const int& opacity) {
  if (livox_viz) {
    livox_viz->updateOpacity(opacity);
  }
  osg_widget->update();
}

void VisualAreaWidget::onChangeBgColorRequested(const QColor& color) {
  osg_widget->getOsgViewer()->getCamera()->setClearColor(osg::Vec4(color.redF(), color.greenF(), color.blueF(), 1.0f));
  osg_widget->update();
}

VisualAreaWidget::~VisualAreaWidget() {
  delete ui;
  delete osg_widget;
  osg_widget = nullptr;
}

void VisualAreaWidget::onCloudFrameReady(const GeneralCloudFrame& frame) {
  if (livox_viz) {
    livox_viz->updateCloud(frame.points);
  }
  osg_widget->update();
}

void VisualAreaWidget::onOdomFrameReady(const OdomFrame& frame) {
  if (cam_viz) {
    cam_viz->updatePose(frame.pose.x, frame.pose.y, frame.pose.z,
        frame.pose.qx, frame.pose.qy, frame.pose.qz, frame.pose.qw);
  }

  if (path_viz) {
    path_viz->updatePose(frame.pose.x, frame.pose.y, frame.pose.z, frame.index);
  }
  osg_widget->update();
}
