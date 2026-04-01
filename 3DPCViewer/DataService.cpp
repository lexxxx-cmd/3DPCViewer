#include "DataService.h"

DataService::DataService(QObject *parent)
	: QObject(parent)
{
	m_workerThread = new QThread(this);
	m_bagWorker = new BagWorker();
	m_bagWorker->moveToThread(m_workerThread);

	connect(m_workerThread, &QThread::finished, m_bagWorker, &QObject::deleteLater);
	connect(m_bagWorker, &QObject::destroyed, [this]() {
		m_bagWorker = nullptr;
		});

	connect(m_bagWorker, &BagWorker::cloudFrameReady, this, &DataService::cloudFrameReady);
	connect(m_bagWorker, &BagWorker::imageFrameReady, this, &DataService::imageFrameReady);
	connect(m_bagWorker, &BagWorker::odomFrameReady, this, &DataService::odomFrameReady);
	connect(m_bagWorker, &BagWorker::progressUpdated, this, &DataService::progressUpdated);

	if (!m_workerThread->isRunning()) {
		m_workerThread->start();
	}
}

DataService::~DataService()
{
	if (m_bagWorker) {
		m_bagWorker->stopProcessing();
	}
	m_workerThread->quit();
	m_workerThread->wait();
}

void DataService::startProcess(const QString& path)
{
	if (m_bagWorker) {
		QMetaObject::invokeMethod(m_bagWorker, "processBag",
			Qt::QueuedConnection,
			Q_ARG(QString, path));
	}
}

void DataService::stopProcess()
{
	///.....
}
