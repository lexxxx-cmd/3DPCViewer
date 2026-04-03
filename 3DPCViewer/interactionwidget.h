#pragma once

#include <QWidget>
#include <memory>
#include <QTimer>
#include "ui_interactionwidget.h"

QT_BEGIN_NAMESPACE
namespace Ui { class InteractionWidgetClass; };
QT_END_NAMESPACE

class InteractionWidget : public QWidget
{
	Q_OBJECT

public:
	InteractionWidget(QWidget *parent = nullptr);
	~InteractionWidget();

public slots:
	void onSizeSliderChanged(int value);
	void onOpacitySliderChanged(int value);
	void onMaxmessageNumSet(int value);
	void onProgressNumChanged(int value);


signals:
	void pointSizeChanged(const int& value);
	void pointOpacityChanged(const int& value);
	void progressUpdated(const int value);


private:
	std::unique_ptr<Ui::InteractionWidgetClass> ui;
	QTimer* m_timer;
	int max_messageNum = 0;
	int cur_messageNum = 0;
	bool isPlay = false;
};

