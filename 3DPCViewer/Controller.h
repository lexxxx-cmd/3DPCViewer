#pragma once

#include <QObject>
#include <iostream>
#include <memory>
#include "pcviewer.h"
#include "DataService.h"

class Controller : public QObject
{
	Q_OBJECT

public:
	Controller(QObject *parent);
	~Controller();


	void run();

private:
	void setupConnections();

	std::unique_ptr<PCViewer> m_viewer;
	std::unique_ptr<DataService> m_dataService;
};
