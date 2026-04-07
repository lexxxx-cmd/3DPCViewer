#pragma once

#include <QWidget>
#include <memory>
#include "ui_ControlPanelWidget.h"
#include "ui/DataWidget.h"
#include "ui/StatusWidget.h"
#include "ui/InteractionWidget.h"

QT_BEGIN_NAMESPACE
namespace Ui { class ControlPanelWidgetClass; };
QT_END_NAMESPACE

class ControlPanelWidget : public QWidget
{
	Q_OBJECT

public:
	ControlPanelWidget(QWidget *parent = nullptr);
	~ControlPanelWidget();
	DataWidget* getDataWidget() const { return ui->DataW; }
	StatusWidget* getStatusWidget() const { return ui->StatusW; }
	InteractionWidget* getInteractionWidget() const { return ui->InterWidget; }

public slots:
	void onFileSizeUpdated(const int& size); // 接收上层：文件大小更新了
	void onPointSizeUpdated(const int& num); // 接收上层：点云数量更新了

signals:
	void requestLoadFile(const QString& path);
	void requestProcBag(const QString& path);// 通知上层：用户选了文件
	void requestUpdateFileSize(const int& size); // 通知下层：文件大小更新了
	void requestUpdatePointSize(const int& num); // 通知下层：点云数量更新了
	void requestUpdateFPS(const int& fps); // 通知下层：FPS更新了
	void requestShowNormals(const bool& show);

	void pointSizeChanged(const int& value);
	void pointOpacityChanged(const int& value);

	void topicListReady(const std::vector<std::string>& topics);
	void messageNumReady(int num);
	void progressUpdated(const int value);//通知上层，显示进度更新

private:
	std::unique_ptr<Ui::ControlPanelWidgetClass> ui;
};

