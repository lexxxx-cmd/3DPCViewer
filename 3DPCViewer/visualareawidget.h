// 3. visualareawidget.h
#ifndef VISUALAREAWIDGET_H
#define VISUALAREAWIDGET_H

#include <QWidget>
#include <osgQOpenGL/osgQOpenGLWidget>
#include <osgViewer/Viewer>
#include <osgGA/TrackballManipulator>
#include <osg/Group>
#include <osg/Geometry>
#include <osg/Point>
#include <QFutureWatcher>

#include "CloudData.h"

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
    void onDataLoaded(); // 异步加载完成后的回调

private:
    void initOSG();
    void updateOsgScene(const CloudData& data);

    Ui::VisualAreaWidgetClass* ui;
    osgQOpenGLWidget* m_osgWidget;

    osg::ref_ptr<osg::Group> m_root;
    osg::ref_ptr<osg::Geometry> m_cloudGeom;

    QFutureWatcher<CloudData> m_loadWatcher;
};

#endif