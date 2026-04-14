#pragma once

#include <QWidget>
#include <QColor>
#include <QFutureWatcher>
#include <osgViewer/Viewer>
#include <osgGA/TrackballManipulator>
#include <osg/Group>
#include <osg/Geometry>
#include <osg/Geode>
#include <osg/Point>
#include <pcl/PCLPointCloud2.h>

#include "osgQOpenGL/osgQOpenGLWidget.h"
#include "io/BagDataTypes.h"
#include "node/OdomCameraVisualizer.h"
#include "node/OdomPathVisualizer.h"
#include "node/LivoxCloudVisualizer.h"
#include "node/GridVisualizer.h"
#include "ImagePanelWidget.h"

namespace Ui { class VisualAreaWidgetClass; }

class VisualAreaWidget : public QWidget {
  Q_OBJECT

 public:
  VisualAreaWidget(QWidget* parent = nullptr);
  ~VisualAreaWidget();

 public slots:
  void onChangeSizeRequested(const int& size);
  void onChangeOpacityRequested(const int& opacity);
  void onChangeBgColorRequested(const QColor& color);

  void onCloudFrameReady(const GeneralCloudFrame& frame);
  void onOdomFrameReady(const OdomFrame& frame);

  void clear();

 signals:

 private:
  void initOsg();
  void updateCloudGeometry(osg::ref_ptr<osg::Geometry> geom, bool is_first_load);

  Ui::VisualAreaWidgetClass* ui;
  osgQOpenGLWidget* osg_widget;

  osg::ref_ptr<osg::Group> root;
  osg::ref_ptr<osg::Geode> cloud_geode;
  osg::ref_ptr<osg::Geometry> cloud_geom;

  pcl::PCLPointCloud2::Ptr current_cloud;

  std::unique_ptr<OdomCameraVisualizer> cam_viz;
  std::unique_ptr<OdomPathVisualizer> path_viz;
  std::unique_ptr<LivoxCloudVisualizer> livox_viz;
  std::unique_ptr<Grid> grid_viz;
  bool show_normals = false;
};
