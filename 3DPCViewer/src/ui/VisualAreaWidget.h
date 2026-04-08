#ifndef VISUALAREAWIDGET_H
#define VISUALAREAWIDGET_H

#include <QWidget>
#include "osgQOpenGL/osgQOpenGLWidget.h"
#include <osgViewer/Viewer>
#include <osgGA/TrackballManipulator>
#include <osg/Group>
#include <osg/Geometry>
#include <osg/Geode>
#include <osg/Point>
#include <QFutureWatcher>

#include <pcl/PCLPointCloud2.h>
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

    void onCloudFrameReady(const GeneralCloudFrame& frame);
    void onOdomFrameReady(const OdomFrame& frame);

signals:

private:
    void initOSG();
    void updateCloudGeometry(osg::ref_ptr<osg::Geometry> geom, bool isFirstLoad);

    Ui::VisualAreaWidgetClass* ui;
    osgQOpenGLWidget* m_osgWidget;

    osg::ref_ptr<osg::Group> m_root;
    osg::ref_ptr<osg::Geode> m_cloudGeode;
    osg::ref_ptr<osg::Geometry> m_cloudGeom;

    pcl::PCLPointCloud2::Ptr m_currentCloud;

    
    std::unique_ptr<OdomCameraVisualizer> _camViz;
    std::unique_ptr<OdomPathVisualizer> _pathViz;
    std::unique_ptr<LivoxCloudVisualizer> _livoxViz;
    std::unique_ptr<Grid> _gridViz;
    // »º´æµ±Ç°×´Ì¬£¬±ãÓÚÔÚÌæ»» Geometry Ê±»Ö¸´
    bool m_showNormals = false;

};

#endif