#include "DatabaseWorker.h"

#include <QSqlError>
#include <QVariant>
#include <QDebug>

DatabaseWorker::DatabaseWorker(QObject* parent)
    : QObject(parent)
{}

DatabaseWorker::~DatabaseWorker()
{
    flushBuffer();
    if (m_db.isOpen()) {
        m_db.close();
    }
    QSqlDatabase::removeDatabase(QString::fromLatin1(DB_CONN_NAME));
}

// ---------------------------------------------------------------------------
// initDatabase  –  must run in the worker thread
// ---------------------------------------------------------------------------
void DatabaseWorker::initDatabase(const QString& dbPath)
{
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                     QString::fromLatin1(DB_CONN_NAME));
    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        qWarning() << "DatabaseWorker: failed to open" << dbPath
                   << m_db.lastError().text();
        return;
    }

    QSqlQuery q(m_db);

    // -----------------------------------------------------------------------
    // ROS2 standard tables (fields must not be altered)
    // -----------------------------------------------------------------------
    q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS topics ("
        "  id                    INTEGER PRIMARY KEY,"
        "  name                  TEXT    NOT NULL,"
        "  type                  TEXT    NOT NULL,"
        "  serialization_format  TEXT    NOT NULL,"
        "  offered_qos_profiles  TEXT    NOT NULL"
        ");"));

    q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS messages ("
        "  id        INTEGER PRIMARY KEY,"
        "  topic_id  INTEGER NOT NULL,"
        "  timestamp INTEGER NOT NULL,"
        "  data      BLOB    NOT NULL"
        ");"));

    // -----------------------------------------------------------------------
    // Custom extension table: tracks which .bag file each bag_index came from
    // -----------------------------------------------------------------------
    q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS internal_bag_info ("
        "  bag_index   INTEGER PRIMARY KEY,"
        "  file_path   TEXT NOT NULL,"
        "  import_time DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");"));

    // Index to speed up time-range queries
    q.exec(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS timestamp_idx ON messages (timestamp);"));

    m_initialized = true;
    qDebug() << "DatabaseWorker: database initialized at" << dbPath;
}

// ---------------------------------------------------------------------------
// saveMessage
// ---------------------------------------------------------------------------
void DatabaseWorker::saveMessage(const RawBagMessage& msg)
{
    if (!m_initialized) {
        qWarning() << "DatabaseWorker: not initialized, dropping message for"
                   << msg.topicName;
        return;
    }

    // Embed the bag index into the topic name so the ROS2 schema stays clean.
    const QString virtualTopic =
        QStringLiteral("/bag%1%2").arg(msg.bagIndex).arg(msg.topicName);

    const int topicId = getOrRegisterTopicId(virtualTopic, msg.typeName);

    m_messageBuffer.append({topicId, msg.timestampNs, msg.rawData});
    if (m_messageBuffer.size() >= BATCH_SIZE) {
        commitBatch();
    }
}

// ---------------------------------------------------------------------------
// flushBuffer
// ---------------------------------------------------------------------------
void DatabaseWorker::flushBuffer()
{
    commitBatch();
}

// ---------------------------------------------------------------------------
// private helpers
// ---------------------------------------------------------------------------
void DatabaseWorker::commitBatch()
{
    if (m_messageBuffer.isEmpty()) {
        return;
    }

    m_db.transaction();
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO messages (topic_id, timestamp, data) VALUES (?, ?, ?)"));

    for (const PendingMessage& pm : m_messageBuffer) {
        q.addBindValue(pm.topicId);
        q.addBindValue(pm.timestampNs);
        q.addBindValue(pm.data);
        if (!q.exec()) {
            qWarning() << "DatabaseWorker: insert failed:" << q.lastError().text();
        }
    }

    if (!m_db.commit()) {
        qWarning() << "DatabaseWorker: commit failed:" << m_db.lastError().text();
        m_db.rollback();
    }

    qDebug() << "DatabaseWorker: committed" << m_messageBuffer.size() << "messages";
    m_messageBuffer.clear();
}

// ---------------------------------------------------------------------------
// loadBagFromDB  –  read back all messages for one bag and emit bagCacheReady
// ---------------------------------------------------------------------------
void DatabaseWorker::loadBagFromDB(int bagIndex)
{
    if (!m_initialized) {
        qWarning() << "DatabaseWorker::loadBagFromDB: not initialized";
        return;
    }

    // First, find all topics whose name starts with /bag{N}/.
    const QString prefix = QStringLiteral("/bag%1/").arg(bagIndex);

    QSqlQuery topicQ(m_db);
    topicQ.prepare(QStringLiteral(
        "SELECT id, name, type FROM topics WHERE name LIKE ?"));
    topicQ.addBindValue(prefix + QLatin1Char('%'));
    if (!topicQ.exec()) {
        qWarning() << "DatabaseWorker::loadBagFromDB: topic query failed:"
                   << topicQ.lastError().text();
        return;
    }

    // Build topicId → (rawTopicName, typeName) map.
    // rawTopicName strips the /bag{N} prefix: "/bag1/livox/lidar" → "/livox/lidar"
    struct TopicMeta { QString rawName; QString typeName; };
    QHash<int, TopicMeta> topicMap;
    QStringList idStrings;

    while (topicQ.next()) {
        const int     id       = topicQ.value(0).toInt();
        const QString fullName = topicQ.value(1).toString();
        const QString typeName = topicQ.value(2).toString();
        // Strip "/bag{N}" prefix (everything up to the second '/').
        const int secondSlash = fullName.indexOf(QLatin1Char('/'), 1);
        const QString rawName = (secondSlash >= 0) ? fullName.mid(secondSlash)
                                                    : fullName;
        topicMap[id] = { rawName, typeName };
        idStrings << QString::number(id);
    }

    if (topicMap.isEmpty()) {
        qWarning() << "DatabaseWorker::loadBagFromDB: no topics found for bag"
                   << bagIndex;
        return;
    }

    // Load all messages for the found topics, ordered by timestamp.
    QSqlQuery msgQ(m_db);
    msgQ.exec(QStringLiteral(
        "SELECT topic_id, timestamp, data FROM messages "
        "WHERE topic_id IN (%1) ORDER BY timestamp ASC")
        .arg(idStrings.join(QLatin1Char(','))));

    if (msgQ.lastError().isValid()) {
        qWarning() << "DatabaseWorker::loadBagFromDB: message query failed:"
                   << msgQ.lastError().text();
        return;
    }

    BagCacheData cache;

    while (msgQ.next()) {
        const int        topicId   = msgQ.value(0).toInt();
        const qint64     timestamp = msgQ.value(1).toLongLong();
        const QByteArray rawData   = msgQ.value(2).toByteArray();

        auto it = topicMap.find(topicId);
        if (it == topicMap.end()) continue;

        const std::string rawName  = it->rawName.toStdString();
        const std::string typeName = it->typeName.toStdString();

        // Record the type (same for every message of this topic).
        cache.types[rawName] = typeName;

        // Strip the 4-byte DDS-CDR encapsulation header that was added
        // by CDRConverter::packRawBytesToCDR when the data was saved.
        if (rawData.size() <= 4) continue;
        const auto* payloadPtr = reinterpret_cast<const uint8_t*>(
            rawData.constData()) + 4;
        const size_t payloadSize = static_cast<size_t>(rawData.size()) - 4;

        cache.payloads[rawName].push_back(
            std::vector<uint8_t>(payloadPtr, payloadPtr + payloadSize));
        cache.timestamps[rawName].push_back(timestamp);

        const int sz = static_cast<int>(cache.payloads[rawName].size());
        if (sz > cache.maxSize) cache.maxSize = sz;
    }

    if (cache.maxSize == 0) {
        qWarning() << "DatabaseWorker::loadBagFromDB: no messages found for bag"
                   << bagIndex;
        return;
    }

    qDebug() << "DatabaseWorker::loadBagFromDB: loaded" << cache.maxSize
             << "max messages for bag" << bagIndex;
    emit bagCacheReady(cache);
}
                                          const QString& typeName)
{
    // Fast path: local cache
    auto it = m_topicIdCache.find(topicName);
    if (it != m_topicIdCache.end()) {
        return it.value();
    }

    // Check whether the topic already exists in the DB
    QSqlQuery sel(m_db);
    sel.prepare(QStringLiteral("SELECT id FROM topics WHERE name = ?"));
    sel.addBindValue(topicName);
    sel.exec();
    if (sel.next()) {
        const int id = sel.value(0).toInt();
        m_topicIdCache[topicName] = id;
        return id;
    }

    // Insert a new row
    QSqlQuery ins(m_db);
    ins.prepare(QStringLiteral(
        "INSERT INTO topics (name, type, serialization_format, offered_qos_profiles)"
        " VALUES (?, ?, 'cdr', '')"));
    ins.addBindValue(topicName);
    ins.addBindValue(typeName);
    if (!ins.exec()) {
        qWarning() << "DatabaseWorker: topic insert failed:" << ins.lastError().text();
        return -1;
    }

    const int id = ins.lastInsertId().toInt();
    m_topicIdCache[topicName] = id;
    return id;
}
