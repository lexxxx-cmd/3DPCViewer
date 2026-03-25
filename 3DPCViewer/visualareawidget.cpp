
#include "visualareawidget.h"
#include <iostream>
#include <QFileInfo>
#include <QMessageBox>

#include <pcl/io/pcd_io.h>
#include <pcl/features/normal_3d.h>
#include <pcl/filters/voxel_grid.h>



static pcl::PointCloud<pcl::PointXYZ>::Ptr loadCloudThread(const QString& path) {
	pcl::PointCloud<pcl::PointXYZ>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZ>);
	if (pcl::io::loadPCDFile<pcl::PointXYZ>(path.toLocal8Bit().constData(), *cloud) == -1) {
		return nullptr;
	}
	return cloud;
}

static pcl::PointCloud<pcl::PointXYZ>::Ptr sampleCloud(pcl::PointCloud<pcl::PointXYZ>::Ptr m_cloud, float leafSize) {
	if (!m_cloud || m_cloud->empty()) return nullptr;
	pcl::PointCloud<pcl::PointXYZ>::Ptr m_cloud2show(new pcl::PointCloud<pcl::PointXYZ>);

	pcl::VoxelGrid<pcl::PointXYZ> sor;
	sor.setInputCloud(m_cloud);
	sor.setLeafSize(leafSize, leafSize, leafSize);
	sor.filter(*m_cloud2show);
	return m_cloud2show;
}

// 异步计算
static pcl::PointCloud<pcl::Normal>::Ptr computeNormalsThread(pcl::PointCloud<pcl::PointXYZ>::Ptr cloud) {
	pcl::PointCloud<pcl::Normal>::Ptr normals(new pcl::PointCloud<pcl::Normal>);
	if (!cloud || cloud->empty()) return normals;

	pcl::NormalEstimation<pcl::PointXYZ, pcl::Normal> ne;
	ne.setInputCloud(cloud);
	pcl::search::KdTree<pcl::PointXYZ>::Ptr tree(new pcl::search::KdTree<pcl::PointXYZ>());
	ne.setSearchMethod(tree);
	ne.setKSearch(20);
	ne.compute(*normals);
	return normals;
}


VisualAreaWidget::VisualAreaWidget(QWidget *parent)
	: QWidget(parent), m_cloud(nullptr), m_cloud2show(nullptr)
{
	ui = std::make_unique< Ui::VisualAreaWidgetClass>();
	ui->setupUi(this);
	// 添加
	m_vtkWidget = new QVTKOpenGLNativeWidget();
	m_vtkWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	ui->gridLayout_2->addWidget(m_vtkWidget);

	auto renderWindow = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
	auto renderer = vtkSmartPointer<vtkRenderer>::New();
	renderWindow->AddRenderer(renderer);
	m_vtkWidget->setRenderWindow(renderWindow);

	m_viewer.reset(new pcl::visualization::PCLVisualizer(
        renderer, 
        renderWindow, 
        "viewer", 
        false
    ));

    // 4. 设置交互器（关键：PCL需要知道Qt提供的交互器）
    m_viewer->setupInteractor(m_vtkWidget->interactor(), m_vtkWidget->renderWindow());
    
    // 5. 初始配置
    m_viewer->setBackgroundColor(0.1, 0.1, 0.1);
    m_vtkWidget->renderWindow()->Render();


	connect(&m_loadWatcher, &QFutureWatcher<pcl::PointCloud<pcl::PointXYZ>::Ptr>::finished,
		this, &VisualAreaWidget::onPointCloudLoaded);
	connect(&m_normalWatcher, &QFutureWatcher<pcl::PointCloud<pcl::Normal>::Ptr>::finished,
		this, &VisualAreaWidget::onNormalsComputed);
	connect(&m_sampleWatcher, &QFutureWatcher<pcl::PointCloud<pcl::PointXYZ>::Ptr>::finished,
		this, &VisualAreaWidget::onCloudSampled);
}

VisualAreaWidget::~VisualAreaWidget() = default;

void VisualAreaWidget::onOpenFileRequested(const QString& path) {
	//耗时加载点云操作
	std::cout << "open" << "/n";
	// 防止重复触发加载
	if (m_loadWatcher.isRunning()) return;

	// TODO: 可在此处向主UI发送信号，显示Loading转圈或进度条
	std::cout << "Start loading point cloud asynchronously..." << std::endl;

	// 启动异步线程加载
	QFuture<pcl::PointCloud<pcl::PointXYZ>::Ptr> future = QtConcurrent::run(loadCloudThread, path);
	m_loadWatcher.setFuture(future);

	// 获取文件大小
	QFileInfo fileInfo(path);
	emit sendFileSize(fileInfo.size() / 1024 / 1024); // KB
}

void VisualAreaWidget::onUpdateDisplayCloud(float leafSize) {
	if (!m_cloud || m_cloud->empty()) return;
	if (m_sampleWatcher.isRunning()) return; 
	QFuture<pcl::PointCloud<pcl::PointXYZ>::Ptr> future = QtConcurrent::run(sampleCloud, m_cloud, leafSize);
	m_sampleWatcher.setFuture(future);

}

void VisualAreaWidget::onChangeSizeRequested(const int& size) {
	//更改点云大小操作
	// 1. 检查渲染器和点云 ID 是否存在
	if (!m_viewer || m_cloudId.isEmpty()) return;

	// 2. 更新点云属性
	// 参数含义：属性类型, 尺寸数值, 点云的 ID
	m_viewer->setPointCloudRenderingProperties(
		pcl::visualization::PCL_VISUALIZER_POINT_SIZE,
		static_cast<double>(size),
		m_cloudId.toStdString()
	);

	// 3. 关键步骤：通知 VTK 窗口立即重新渲染，否则画面不会变化
	m_vtkWidget->renderWindow()->Render();

}

void VisualAreaWidget::onChangeOpacityRequested(const int& opacity) {
	//更改点云透明度操作
	if (!m_viewer || m_cloudId.isEmpty()) return;

	// 将整数范围 (0-100) 映射到 (0.0-1.0)
	double opacityValue = opacity / 100.0;

	m_viewer->setPointCloudRenderingProperties(
		pcl::visualization::PCL_VISUALIZER_OPACITY,
		opacityValue,
		m_cloudId.toStdString()
	);

	m_vtkWidget->renderWindow()->Render();

}

void VisualAreaWidget::onShowNormalsRequested(const bool& show) {
	//显示法线操作
}

void VisualAreaWidget::onPointCloudLoaded() {
	m_cloud = m_loadWatcher.result();
	
	if (!m_cloud || m_cloud->empty()) {
		QMessageBox::warning(this, "Error", "Failed to load point cloud file.");
		return;
	}
	onUpdateDisplayCloud(0.01f);
}

void VisualAreaWidget::onCloudSampled() {
	m_cloud2show = m_sampleWatcher.result();
	if (!m_cloud2show || m_cloud2show->empty()) {
		QMessageBox::warning(this, "Error", "Failed to load point cloud file.");
		return;
	}
	// 清空现有视图并渲染新点云
	m_viewer->removeAllPointClouds();
	m_viewer->addPointCloud<pcl::PointXYZ>(m_cloud2show, m_cloudId.toStdString());
	m_viewer->resetCamera();
	m_vtkWidget->renderWindow()->Render();

	// 更新UI信息
	emit sendPointSize(static_cast<int>(m_cloud2show->points.size()));
	std::cout << "Loading complete. Points: " << m_cloud2show->points.size() << std::endl;
}

void VisualAreaWidget::onNormalsComputed() {

}