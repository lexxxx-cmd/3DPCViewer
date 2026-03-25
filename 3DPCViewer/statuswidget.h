#pragma once

#include <QWidget>
#include <memory>
#include "ui_statuswidget.h"

QT_BEGIN_NAMESPACE
namespace Ui { class StatusWidgetClass; };
QT_END_NAMESPACE

class StatusWidget : public QWidget
{
	Q_OBJECT

public:
	StatusWidget(QWidget *parent = nullptr);
	~StatusWidget();


public slots:
	void updatePointSize(const int& size);
	void updateFPS(const int& fps);


private:
	std::unique_ptr<Ui::StatusWidgetClass> ui;
};

