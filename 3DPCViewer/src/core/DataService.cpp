#include "DataService.h"
#include "db/DatabaseWorker.h"

DataService::DataService(QObject *parent)
: QObject(parent)
{
// Register custom meta-types so they can be used in queued signal/slot
// connections across threads.
qRegisterMetaType<RawBagMessage>("RawBagMessage");
qRegisterMetaType<BagCacheData>("BagCacheData");
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

connect(m_bagWorker, &BagWorker::cloudFrameReady,  this, &DataService::cloudFrameReady);
connect(m_bagWorker, &BagWorker::imageFrameReady,  this, &DataService::imageFrameReady);
connect(m_bagWorker, &BagWorker::odomFrameReady,   this, &DataService::odomFrameReady);
connect(m_bagWorker, &BagWorker::progressUpdated,  this, &DataService::progressUpdated);
connect(m_bagWorker, &BagWorker::topicListReady,   this, &DataService::topicListReady);
connect(m_bagWorker, &BagWorker::messageNumReady,  this, &DataService::messageNumReady);
connect(m_bagWorker, &BagWorker::finished,         this, &DataService::finished);

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

// When DatabaseWorker finishes loading a bag from the DB, pass the
// reconstructed cache to BagWorker so it can resume slider-based playback.
connect(m_dbWorker, &DatabaseWorker::bagCacheReady,
        m_bagWorker, &BagWorker::receiveBagCache,
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
if (m_bagWorker) {
QMetaObject::invokeMethod(m_bagWorker, "processBag",
Qt::QueuedConnection,
Q_ARG(QString, path),
Q_ARG(int, m_nextBagIndex++));
}
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

// ---------------------------------------------------------------------------
// onTopicSelected
// ---------------------------------------------------------------------------

namespace {
// Length of the "/bag" prefix in a prefixed topic name like "/bag1/livox/lidar".
static constexpr int BAG_PREFIX_LENGTH = 4; // "/bag"

// Returns the raw topic name from a prefixed topic name.
// e.g. "/bag1/livox/lidar" → "/livox/lidar"
QString extractRawTopicName(const QString& prefixedTopic)
{
    const int secondSlash = prefixedTopic.indexOf(QLatin1Char('/'), 1);
    return (secondSlash >= 0) ? prefixedTopic.mid(secondSlash) : prefixedTopic;
}
} // namespace

void DataService::onTopicSelected(const QString& prefixedTopic, bool checked)
{
    // Parse the bag index from the prefix: "/bag1/livox/lidar" → bagIndex = 1
    // The format is /bag{N}/rest, so the bag prefix is the part before the
    // second '/'.
    const int secondSlash = prefixedTopic.indexOf(QLatin1Char('/'), 1);
    if (secondSlash < 0) return; // malformed topic name

    const QString bagPart  = prefixedTopic.mid(BAG_PREFIX_LENGTH, secondSlash - BAG_PREFIX_LENGTH);
    const int     bagIndex = bagPart.toInt();
    if (bagIndex <= 0) return;

    if (checked) {
        const bool bagChanged = (m_currentBagIndex != 0 &&
                                 m_currentBagIndex != bagIndex);

        if (bagChanged) {
            // Switching to a different bag: stop any ongoing import and
            // clear the active-topic list for the old bag.
            if (m_bagWorker) {
                m_bagWorker->stopProcessing();
            }
            m_checkedTopics.clear();
        }

        m_currentBagIndex = bagIndex;
        m_checkedTopics.insert(prefixedTopic);

        // Rebuild active (raw) topic list for BagWorker.
        QStringList activeRaw;
        for (const QString& t : m_checkedTopics) {
            activeRaw << extractRawTopicName(t);
        }
        if (m_bagWorker) {
            QMetaObject::invokeMethod(m_bagWorker, "setActiveTopics",
                Qt::QueuedConnection,
                Q_ARG(QStringList, activeRaw));
        }

        // Load (or reload) the bag from the database so the slider-based
        // playback uses the persisted data.
        if (m_dbWorker) {
            QMetaObject::invokeMethod(m_dbWorker, "loadBagFromDB",
                Qt::QueuedConnection,
                Q_ARG(int, bagIndex));
        }
    } else {
        m_checkedTopics.remove(prefixedTopic);

        QStringList activeRaw;
        for (const QString& t : m_checkedTopics) {
            activeRaw << extractRawTopicName(t);
        }
        if (m_bagWorker) {
            QMetaObject::invokeMethod(m_bagWorker, "setActiveTopics",
                Qt::QueuedConnection,
                Q_ARG(QStringList, activeRaw));
        }

        if (m_checkedTopics.isEmpty()) {
            m_currentBagIndex = 0;
        }
    }
}
