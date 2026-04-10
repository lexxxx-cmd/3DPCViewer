#pragma once

#include <QtWidgets/QMainWindow>
#include <memory>
#include "ui_PCViewer.h"
#include "ui/ControlPanelWidget.h"
#include "ui/VisualAreaWidget.h"

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
	void requestProcBag(const QString& path);
	void requestUpdateFileSize(const int& size);
	void requestUpdateFPS(const int& fps); 
	void requestShowNormals(const bool& show);

	void pointSizeChanged(const int& value);
	void pointOpacityChanged(const int& value);

	// 转发service面板数据
	void cloudFrameReady(const GeneralCloudFrame& frame);
	void imageFrameReady(const ImageFrame& frame);
	void odomFrameReady(const OdomFrame& frame);
	void topicListReady(const std::vector<std::string>& topics);
	void messageNumReady(int num);

	void progressUpdated(const int value);

	// 错误
	void errorOccur(const QString& errorMsg);

private:
    std::unique_ptr<Ui::PCViewerClass> ui;

};

