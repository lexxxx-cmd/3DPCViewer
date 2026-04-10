#include "DataService.h"
#include "db/DatabaseWorker.h"
#include <set>

DataService::DataService(QObject *parent)
: QObject(parent)
{
// Register custom meta-types so they can be used in queued signal/slot
// connections across threads.
qRegisterMetaType<RawBagMessage>("RawBagMessage");
qRegisterMetaType<GeneralCloudFrame>("GeneralCloudFrame");
qRegisterMetaType<ImageFrame>("ImageFrame");
qRegisterMetaType<OdomFrame>("OdomFrame");
qRegisterMetaType<std::vector<std::string>>("std::vector<std::string>");

// ------------------------------------------------------------------
// Bag-reading worker thread
// ------------------------------------------------------------------
m_workerThread = new QThread(this);
m_bagWorker    = new BagWorker();
m_bagWorker->moveToThread(m_workerThread);

connect(m_workerThread, &QThread::finished, m_bagWorker, &QObject::deleteLater);
connect(m_bagWorker, &QObject::destroyed, [this]() { m_bagWorker = nullptr; });

connect(m_bagWorker, &BagWorker::cloudFrameReady,  this, &DataService::cloudFrameReady);
connect(m_bagWorker, &BagWorker::imageFrameReady,  this, &DataService::imageFrameReady);
connect(m_bagWorker, &BagWorker::odomFrameReady,   this, &DataService::odomFrameReady);
connect(m_bagWorker, &BagWorker::progressUpdated,  this, &DataService::progressUpdated);
connect(m_bagWorker, &BagWorker::topicListReady,   this, &DataService::topicListReady);
connect(m_bagWorker, &BagWorker::messageNumReady,  this, &DataService::messageNumReady);
connect(m_bagWorker, &BagWorker::finished,         this, &DataService::onBagWorkerFinished);
connect(m_bagWorker, &BagWorker::errorOccur,       this, &DataService::onBagWorkerError);

if (!m_workerThread->isRunning()) {
m_workerThread->start();
}

// ------------------------------------------------------------------
// Database worker thread
// ------------------------------------------------------------------
m_dbThread = new QThread(this);
m_dbWorker = new DatabaseWorker();
m_dbWorker->moveToThread(m_dbThread);

connect(m_dbThread, &QThread::finished, m_dbWorker, &QObject::deleteLater);
connect(m_dbWorker, &QObject::destroyed, [this]() { m_dbWorker = nullptr; });

// Wire the CDR-encoded messages from BagWorker to DatabaseWorker.
// The queued connection ensures thread safety automatically.
connect(m_bagWorker, &BagWorker::parsedMessageReady,
        m_dbWorker,  &DatabaseWorker::saveMessage,
        Qt::QueuedConnection);

// Flush any remaining buffered messages when the bag worker finishes.
connect(m_bagWorker, &BagWorker::finished,
        m_dbWorker,  &DatabaseWorker::flushBuffer,
        Qt::QueuedConnection);

m_dbThread->start();

// Initialise the SQLite database inside the DB thread.
QMetaObject::invokeMethod(m_dbWorker, "initDatabase",
                          Qt::QueuedConnection,
                          Q_ARG(QString, QStringLiteral("temp.db3")));
}

DataService::~DataService()
{
if (m_bagWorker) {
m_bagWorker->stopProcessing();
}
m_workerThread->quit();
m_workerThread->wait();

// Flush remaining DB messages before shutting down the DB thread.
if (m_dbWorker) {
QMetaObject::invokeMethod(m_dbWorker, "flushBuffer",
                          Qt::BlockingQueuedConnection);
}
m_dbThread->quit();
m_dbThread->wait();
}

void DataService::startProcess(const QString& path)
{
if (!m_bagWorker || m_importInProgress) {
return;
}

const int bagIndex = m_nextBagIndex++;
m_currentBagIndex = bagIndex;
m_loadedBagIndex = bagIndex;
m_activeCheckedTopics.clear();
setImportInProgress(true);

QMetaObject::invokeMethod(m_bagWorker,
                          [this]() {
                              if (!m_bagWorker) {
                                  return;
                              }
                              m_bagWorker->setActiveTopics({});
                          },
                          Qt::QueuedConnection);

QMetaObject::invokeMethod(m_bagWorker, "processBag",
Qt::QueuedConnection,
Q_ARG(QString, path),
Q_ARG(int, bagIndex));
}

void DataService::updateProgress(const int value) {
if (m_bagWorker) {
QMetaObject::invokeMethod(m_bagWorker, "updateProgress",
Qt::QueuedConnection,
Q_ARG(int, value));
}
}

void DataService::stopProcess()
{
if (m_bagWorker) {
m_bagWorker->stopProcessing();
}
}

void DataService::loadBagFromDatabase(int bagIndex)
{
if (!m_bagWorker || !m_dbWorker || m_importInProgress || bagIndex <= 0 || bagIndex == m_loadedBagIndex) {
return;
}

setImportInProgress(true);
m_currentBagIndex = bagIndex;
m_loadedBagIndex = bagIndex;

QMetaObject::invokeMethod(m_dbWorker,
                          [this, bagIndex]() {
                              std::vector<RawBagMessage> dbMessages =
                                  m_dbWorker->loadMessagesByBagIndex(bagIndex);

                              QMetaObject::invokeMethod(this,
                                                        [this, bagIndex, dbMessages = std::move(dbMessages)]() mutable {
                                                            if (!m_bagWorker) {
                                                                setImportInProgress(false);
                                                                return;
                                                            }
                                                            QMetaObject::invokeMethod(
                                                                m_bagWorker,
                                                                [this, bagIndex, dbMessages = std::move(dbMessages)]() mutable {
                                                                    m_bagWorker->rebuildCacheFromDbMessages(dbMessages, bagIndex);
                                                                },
                                                                Qt::QueuedConnection);
                                                        },
                                                        Qt::QueuedConnection);
                          },
                          Qt::QueuedConnection);
}

void DataService::setImportInProgress(bool inProgress)
{
if (m_importInProgress == inProgress) {
return;
}
m_importInProgress = inProgress;
emit importStateChanged(m_importInProgress);
}

void DataService::onBagNodeActivated(int bagIndex)
{
if (!m_bagWorker || bagIndex <= 0 || m_importInProgress) {
return;
}

if (bagIndex == m_currentBagIndex) {
return;
}

m_currentBagIndex = bagIndex;
m_activeCheckedTopics.clear();
QMetaObject::invokeMethod(m_bagWorker,
                          [this]() {
                              if (!m_bagWorker) {
                                  return;
                              }
                              m_bagWorker->setActiveTopics({});
                          },
                          Qt::QueuedConnection);

loadBagFromDatabase(bagIndex);
}

void DataService::onTopicSelectionChanged(int bagIndex, const std::vector<std::string>& checkedRawTopics)
{
if (!m_bagWorker || bagIndex <= 0 || bagIndex != m_currentBagIndex || bagIndex != m_loadedBagIndex) {
return;
}

const std::set<std::string> nextCheckedTopics(checkedRawTopics.begin(), checkedRawTopics.end());
if (nextCheckedTopics == m_activeCheckedTopics) {
return;
}

m_activeCheckedTopics = nextCheckedTopics;
QMetaObject::invokeMethod(m_bagWorker,
                          [this, checkedRawTopics]() {
                              if (!m_bagWorker) {
                                  return;
                              }
                              m_bagWorker->setActiveTopics(checkedRawTopics);
                          },
                          Qt::QueuedConnection);
}

void DataService::onBagWorkerFinished()
{
setImportInProgress(false);
emit finished();
}

void DataService::onBagWorkerError(const QString& errorMsg)
{
setImportInProgress(false);
emit errorOccur(errorMsg);
}
