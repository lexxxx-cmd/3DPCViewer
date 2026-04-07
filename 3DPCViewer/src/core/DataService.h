#pragma once

#include <QObject>
#include <QThread>
#include "io/BagWorker.h"

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
	void updateProgress(const int value);
	void stopProcess();

signals:
	// 将解析好的数据抛给前端
	void cloudFrameReady(const GeneralCloudFrame& frame);
	void imageFrameReady(const ImageFrame& frame);
	void odomFrameReady(const OdomFrame& frame);
	void progressUpdated(int percent);
	void topicListReady(const std::vector<std::string>& topics);
	void messageNumReady(int num);

	// 错误
	void errorOccur(const QString& errorMsg);

	// 任务结束信号
	void finished();

private:
	BagWorker* m_bagWorker;
	QThread* m_workerThread;
};
