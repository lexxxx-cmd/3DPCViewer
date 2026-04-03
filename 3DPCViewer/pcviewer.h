#pragma once

#include <QtWidgets/QMainWindow>
#include <memory>
#include "ui_pcviewer.h"
#include "controlpanelwidget.h"
#include "visualareawidget.h"

QT_BEGIN_NAMESPACE
namespace Ui { class PCViewerClass; };
QT_END_NAMESPACE

class PCViewer : public QMainWindow
{
    Q_OBJECT

public:
    PCViewer(QWidget *parent = nullptr);
    ~PCViewer();

    ControlPanelWidget* getControlPanel() const { return ui->ControlWidget; }
    VisualAreaWidget* getVisualPanel() const { return ui->ShowWidget; }

public slots:

signals:
	// control面板
	void requestLoadFile(const QString& path);
	void requestProcBag(const QString& path);// 通知上层：用户选了文件
	void requestUpdateFileSize(const int& size); // 通知下层：文件大小更新了
	void requestUpdatePointSize(const int& num); // 通知下层：点云数量更新了
	void requestUpdateFPS(const int& fps); // 通知下层：FPS更新了
	void requestShowNormals(const bool& show);

	void pointSizeChanged(const int& value);
	void pointOpacityChanged(const int& value);

	// 转发service面板数据
	void cloudFrameReady(const LivoxCloudFrame& frame);
	void imageFrameReady(const ImageFrame& frame);
	void odomFrameReady(const OdomFrame& frame);
	void topicListReady(const std::vector<std::string>& topics);
	void messageNumReady(int num);

	void progressUpdated(const int value);//通知上层，显示进度更新

	// 错误
	void errorOccur(const QString& errorMsg);

private:
    std::unique_ptr<Ui::PCViewerClass> ui;

};

