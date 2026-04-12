#pragma once

#include <QWidget>
#include <memory>
#include "ui_DataWidget.h"

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
	void setImportInProgress(bool inProgress);

signals:
	void requestProcBag(const QString& bagPath); // ֪ͨ�ϲ㣺�û�ѡ�� bag �ļ���׼����ʼ����

private:
	std::unique_ptr<Ui::DataWidgetClass> ui;
};
