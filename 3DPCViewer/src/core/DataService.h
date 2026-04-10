#pragma once

#include <QObject>
#include <QSet>
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
void updateProgress(const int value);
void stopProcess();

// Called when the user checks or unchecks a topic in the status tree.
// prefixedTopic: full name with bag prefix, e.g. "/bag1/livox/lidar".
void onTopicSelected(const QString& prefixedTopic, bool checked);

signals:
void cloudFrameReady(const GeneralCloudFrame& frame);
void imageFrameReady(const ImageFrame& frame);
void odomFrameReady(const OdomFrame& frame);
void progressUpdated(int percent);
void topicListReady(const std::vector<std::string>& topics);
void messageNumReady(int num);
void errorOccur(const QString& errorMsg);
void finished();

private:
BagWorker*      m_bagWorker{nullptr};
QThread*        m_workerThread{nullptr};

DatabaseWorker* m_dbWorker{nullptr};
QThread*        m_dbThread{nullptr};

int      m_nextBagIndex{1};
int      m_currentBagIndex{0};   // bag currently loaded in the cache (0 = none)
QSet<QString> m_checkedTopics;   // prefixed topic names that are checked
};
