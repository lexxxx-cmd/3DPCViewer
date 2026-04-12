#include "DataService.h"
#include "db/DatabaseWorker.h"

DataService::DataService(QObject *parent)
: QObject(parent)
{
// Register custom meta-types so they can be used in queued signal/slot
// connections across threads.
qRegisterMetaType<RawBagMessage>("RawBagMessage");
qRegisterMetaType<std::vector<RawBagMessage>>("std::vector<RawBagMessage>");
qRegisterMetaType<GeneralCloudFrame>("GeneralCloudFrame");
qRegisterMetaType<ImageFrame>("ImageFrame");
qRegisterMetaType<OdomFrame>("OdomFrame");

// ------------------------------------------------------------------
// Bag-reading worker thread
// ------------------------------------------------------------------
m_workerThread = new QThread(this);
m_bagWorker    = new BagWorker();
m_bagWorker->moveToThread(m_workerThread);

connect(m_workerThread, &QThread::finished, m_bagWorker, &QObject::deleteLater);
connect(m_bagWorker, &QObject::destroyed, [this]() { m_bagWorker = nullptr; });

// Forward BagWorker's frame signals to DataService signals
connect(m_bagWorker, &BagWorker::cloudFrameReady,  this, &DataService::cloudFrameReady);
connect(m_bagWorker, &BagWorker::imageFrameReady,  this, &DataService::imageFrameReady);
connect(m_bagWorker, &BagWorker::odomFrameReady,   this, &DataService::odomFrameReady);
connect(m_bagWorker, &BagWorker::topicListReady,   this, &DataService::topicListReady);
connect(m_bagWorker, &BagWorker::messageNumReady,  this, &DataService::messageNumReady);
connect(m_bagWorker, &BagWorker::finished,         this, &DataService::onBagWorkerFinished);
connect(m_bagWorker, &BagWorker::errorOccur,       this, &DataService::onBagWorkerError);

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
// parsedMessageReady now includes msgIndex parameter.
connect(m_bagWorker, &BagWorker::parsedMessageReady,
        m_dbWorker,  [this](const RawBagMessage& msg, int msgIndex) {
            m_dbWorker->saveMessage(msg, msgIndex);
        },
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
setImportInProgress(true);

QMetaObject::invokeMethod(m_bagWorker, "processBag",
Qt::QueuedConnection,
Q_ARG(QString, path),
Q_ARG(int, bagIndex));
}

void DataService::updateProgress(const int msgIndex) {
    if (!m_bagWorker || !m_dbWorker || m_importInProgress) {
        return;
    }

    // Query messages at this index from database
    QMetaObject::invokeMethod(m_dbWorker,
        [this, msgIndex]() {
            std::vector<RawBagMessage> messages =
                m_dbWorker->loadMessagesAt(m_currentBagIndex, msgIndex);

            // Parse the messages on BagWorker thread
            QMetaObject::invokeMethod(m_bagWorker,
                [this, messages = std::move(messages), msgIndex]() mutable {
                    m_bagWorker->parseMessages(messages, msgIndex);
                },
                Qt::QueuedConnection);
        },
        Qt::QueuedConnection);
}

void DataService::stopProcess()
{
if (m_bagWorker) {
m_bagWorker->stopProcessing();
}
}

void DataService::setImportInProgress(bool inProgress)
{
if (m_importInProgress == inProgress) {
return;
}
m_importInProgress = inProgress;
emit importStateChanged(m_importInProgress);
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
