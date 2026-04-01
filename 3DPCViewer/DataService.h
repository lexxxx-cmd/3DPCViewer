#pragma once

#include <QObject>
#include <QThread>
#include "BagWorker.h"

class DataService : public QObject
{
	Q_OBJECT

public:
	DataService(QObject *parent = nullptr);
	~DataService();

	BagWorker* getWorker() const { return m_bagWorker; }
	QThread* getThread() const { return m_workerThread; }
public slots:
	void startProcess(const QString& path);
	void stopProcess();

signals:
	// 将解析好的数据抛给前端
	void cloudFrameReady(const LivoxCloudFrame& frame);
	void imageFrameReady(const ImageFrame& frame);
	void odomFrameReady(const OdomFrame& frame);
	void progressUpdated(int percent);

	// 错误
	void errorOccur(const QString& errorMsg);

	// 任务结束信号
	void finished();

private:
	BagWorker* m_bagWorker;
	QThread* m_workerThread;
};
