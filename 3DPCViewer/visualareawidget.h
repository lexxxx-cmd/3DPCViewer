#pragma once

#include <QWidget>
#include <memory>
#include <vtkGenericOpenGLRenderWindow.h>
#include <QVTKOpenGLNativeWidget.h>
#include <vtkSmartPointer.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/visualization/pcl_visualizer.h>
#include "ui_visualareawidget.h"

#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrent>

QT_BEGIN_NAMESPACE
namespace Ui { class VisualAreaWidgetClass; };
QT_END_NAMESPACE

class VisualAreaWidget : public QWidget
{
	Q_OBJECT

public:
	VisualAreaWidget(QWidget *parent = nullptr);
	~VisualAreaWidget();
public slots:
	void onOpenFileRequested(const QString& path); // 接收上层：用户选了文件
	void onChangeSizeRequested(const int& size); // 接收上层：用户改了点大小
	void onChangeOpacityRequested(const int& opacity); // 接收上层：用户改了点透明度
	void onShowNormalsRequested(const bool& show);
	void onUpdateDisplayCloud(float leafSize);


private slots:
	// 响应点云/法线在子线程处理完成的信号
	void onPointCloudLoaded();
	void onNormalsComputed();
	void onCloudSampled();

signals:
	void sendFileSize(int size);
	void sendPointSize(int num);
	void sendFPS(int fps);

private:
	std::unique_ptr<Ui::VisualAreaWidgetClass> ui;
	QVTKOpenGLNativeWidget* m_vtkWidget;

	std::shared_ptr<pcl::visualization::PCLVisualizer> m_viewer;

	pcl::PointCloud<pcl::PointXYZ>::Ptr m_cloud;
	pcl::PointCloud<pcl::PointXYZ>::Ptr m_cloud2show;
	QString m_cloudId = "cloud_main";
	QString m_normalsId = "cloud_normals";

	// 用于异步监听的 Watcher
	QFutureWatcher<pcl::PointCloud<pcl::PointXYZ>::Ptr> m_loadWatcher;
	QFutureWatcher<pcl::PointCloud<pcl::PointXYZ>::Ptr> m_sampleWatcher;
	QFutureWatcher<pcl::PointCloud<pcl::Normal>::Ptr> m_normalWatcher;
};

