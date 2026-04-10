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
	void onFileSizeUpdated(const int& size); // �����ϲ㣺�ļ���С������
	void onPointSizeUpdated(const int& num); // �����ϲ㣺��������������

signals:
	void requestLoadFile(const QString& path);
	void requestProcBag(const QString& path);// ֪ͨ�ϲ㣺�û�ѡ���ļ�
	void requestUpdateFileSize(const int& size); // ֪ͨ�²㣺�ļ���С������
	void requestUpdatePointSize(const int& num); // ֪ͨ�²㣺��������������
	void requestUpdateFPS(const int& fps); // ֪ͨ�²㣺FPS������
	void requestShowNormals(const bool& show);

	void pointSizeChanged(const int& value);
	void pointOpacityChanged(const int& value);
	void bgColorChanged(const QColor& color);

	void topicListReady(const std::vector<std::string>& topics);
	void messageNumReady(int num);
	void progressUpdated(const int value);//֪ͨ�ϲ㣬��ʾ���ȸ���
	void onImageFrameReady(const ImageFrame& frame);

private:
	std::unique_ptr<Ui::ControlPanelWidgetClass> ui;
};
