#pragma once

#include <QObject>
#include <QThread>
#include "io/BagWorker.h"

class DatabaseWorker;

class DataService : public QObject
{
Q_OBJECT

public:
DataService(QObject *parent = nullptr);
~DataService();

BagWorker* getWorker() const { return m_bagWorker; }
QThread*   getThread() const { return m_workerThread; }

public slots:
void startProcess(const QString& path);
void updateProgress(const int msgIndex);
void stopProcess();

signals:
void cloudFrameReady(const GeneralCloudFrame& frame);
void imageFrameReady(const ImageFrame& frame);
void odomFrameReady(const OdomFrame& frame);
void progressUpdated(int percent);
void topicListReady(const std::vector<std::string>& topics);
void messageNumReady(int num);
void errorOccur(const QString& errorMsg);
void finished();
void importStateChanged(bool inProgress);

private:
void setImportInProgress(bool inProgress);
void onBagWorkerFinished();
void onBagWorkerError(const QString& errorMsg);

BagWorker*      m_bagWorker{nullptr};
QThread*        m_workerThread{nullptr};

DatabaseWorker* m_dbWorker{nullptr};
QThread*        m_dbThread{nullptr};

int m_nextBagIndex{1};
int m_currentBagIndex{0};
int m_maxMessageCount{0};
bool m_importInProgress{false};
};
