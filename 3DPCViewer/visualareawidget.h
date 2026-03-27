#ifndef VISUALAREAWIDGET_H
#define VISUALAREAWIDGET_H

#include <QWidget>
#include <osgQOpenGL/osgQOpenGLWidget>
#include <osgViewer/Viewer>
#include <osgGA/TrackballManipulator>
#include <osg/Group>
#include <osg/Geometry>
#include <osg/Geode>
#include <osg/Point>
#include <QFutureWatcher>

#include <pcl/PCLPointCloud2.h>

namespace Ui { class VisualAreaWidgetClass; }

class VisualAreaWidget : public QWidget {
    Q_OBJECT
public:
    VisualAreaWidget(QWidget* parent = nullptr);
    ~VisualAreaWidget();

public slots:
    void onOpenFileRequested(const QString& path);
    void onChangeSizeRequested(const int& size);
    void onChangeOpacityRequested(const int& opacity);
    void onShowNormalsRequested(const bool& show);

signals:
    void sendFileSize(int size);
    void sendPointSize(int num);

private slots:
    void onDataLoaded();
    void onDataDownsampled();
    void onNormalsComputed();

private:
    void initOSG();
    void updateCloudGeometry(osg::ref_ptr<osg::Geometry> geom, bool isFirstLoad);
    void adaptiveDownsampleAndComputeNormals();

    Ui::VisualAreaWidgetClass* ui;
    osgQOpenGLWidget* m_osgWidget;

    osg::ref_ptr<osg::Group> m_root;
    osg::ref_ptr<osg::Geode> m_cloudGeode;
    osg::ref_ptr<osg::Geode> m_normalGeode;

    osg::ref_ptr<osg::Geometry> m_cloudGeom;
    osg::ref_ptr<osg::Geometry> m_cloudNormalGeom;

    pcl::PCLPointCloud2::Ptr m_currentCloud;

    QFutureWatcher<pcl::PCLPointCloud2::Ptr> m_loadWatcher;
    QFutureWatcher<pcl::PCLPointCloud2::Ptr> m_downsampleWatcher;
    QFutureWatcher<osg::ref_ptr<osg::Geometry>> m_normalWatcher;

    // »º´æµ±Ç°×´Ì¬£¬±ãÓÚÔÚÌæ»» Geometry Ê±»Ö¸´
    int m_currentPointSize = 2;
    int m_currentOpacity = 100;
    bool m_showNormals = false;

    float m_leafSize = 0.05f;
};

#endif