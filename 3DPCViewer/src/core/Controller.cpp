#include "Controller.h"

Controller::Controller(QObject *parent)
	: QObject(parent)
{
	m_viewer = std::make_unique<PCViewer>();
	m_dataService = std::make_unique<DataService>();

	setupConnections();
}

Controller::~Controller()
{
}

void Controller::run()
{
	m_viewer->show();
}

void Controller::setupConnections()
{
    // UI request → DataService
	connect(m_viewer.get(), &PCViewer::requestProcBag, m_dataService.get(), &DataService::startProcess);
	connect(m_viewer.get(), &PCViewer::progressUpdated, m_dataService.get(), &DataService::updateProgress);

    // Topic checkbox → DataService
    connect(m_viewer.get(), &PCViewer::topicSelected,
            m_dataService.get(), &DataService::onTopicSelected);

	// DataService → UI
	connect(m_dataService.get(), &DataService::cloudFrameReady, m_viewer.get(), &PCViewer::cloudFrameReady);
	connect(m_dataService.get(), &DataService::imageFrameReady, m_viewer.get(), &PCViewer::imageFrameReady);
	connect(m_dataService.get(), &DataService::odomFrameReady, m_viewer.get(), &PCViewer::odomFrameReady);
	connect(m_dataService.get(), &DataService::topicListReady, m_viewer.get(), &PCViewer::topicListReady);
	connect(m_dataService.get(), &DataService::messageNumReady, m_viewer.get(), &PCViewer::messageNumReady);
}
