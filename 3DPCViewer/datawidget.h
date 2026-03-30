#pragma once

#include <QWidget>
#include <memory>
#include "ui_datawidget.h"

QT_BEGIN_NAMESPACE
namespace Ui { class DataWidgetClass; };
QT_END_NAMESPACE

class DataWidget : public QWidget
{
	Q_OBJECT

public:
	DataWidget(QWidget *parent = nullptr);
	~DataWidget();

public slots:
	void updateFileSize(const int& size);
signals:
	void requestLoadFile(const QString& path); // 通知上层：用户选了文件
	void requestProcBag(const QString& bagPath); // 通知上层：用户选了 bag 文件，准备开始处理

private:
	std::unique_ptr<Ui::DataWidgetClass> ui;
};

