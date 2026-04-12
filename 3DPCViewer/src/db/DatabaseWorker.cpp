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
        "  msg_index INTEGER NOT NULL,"
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

    // Index for efficient msg_index queries
    q.exec(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS msg_index_idx ON messages (msg_index);"));

    m_initialized = true;
    qDebug() << "DatabaseWorker: database initialized at" << dbPath;
}

// ---------------------------------------------------------------------------
// saveMessage
// ---------------------------------------------------------------------------
void DatabaseWorker::saveMessage(const RawBagMessage& msg, int msgIndex)
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

    m_messageBuffer.append({topicId, msg.timestampNs, msgIndex, msg.rawData});

    // Track max message count per bag
    int currentMax = m_bagMaxMessageCount.value(msg.bagIndex, -1);
    if (msgIndex > currentMax) {
        m_bagMaxMessageCount[msg.bagIndex] = msgIndex;
    }

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
        "INSERT INTO messages (topic_id, timestamp, msg_index, data) VALUES (?, ?, ?, ?)"));

    for (const PendingMessage& pm : m_messageBuffer) {
        q.addBindValue(pm.topicId);
        q.addBindValue(pm.timestampNs);
        q.addBindValue(pm.msgIndex);
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

int DatabaseWorker::getOrRegisterTopicId(const QString& topicName,
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

// ---------------------------------------------------------------------------
// loadMessagesAt
// ---------------------------------------------------------------------------
std::vector<RawBagMessage> DatabaseWorker::loadMessagesAt(int bagIndex, int msgIndex)
{
    std::vector<RawBagMessage> result;
    if (!m_initialized) {
        qWarning() << "DatabaseWorker: not initialized, cannot load messages at index" << msgIndex;
        return result;
    }

    const QString bagPrefix = QStringLiteral("/bag%1/").arg(bagIndex);

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT t.name, t.type, m.timestamp, m.data "
        "FROM messages m "
        "JOIN topics t ON t.id = m.topic_id "
        "WHERE t.name LIKE ? AND m.msg_index = ? "
        "ORDER BY t.name ASC"));
    q.addBindValue(bagPrefix + QStringLiteral("%"));
    q.addBindValue(msgIndex);

    if (!q.exec()) {
        qWarning() << "DatabaseWorker: query messages failed:" << q.lastError().text();
        return result;
    }

    while (q.next()) {
        const QString prefixedTopic = q.value(0).toString();
        QString rawTopic = prefixedTopic;
        if (prefixedTopic.startsWith(bagPrefix)) {
            const QString topicSuffix = prefixedTopic.mid(bagPrefix.size());
            rawTopic = topicSuffix.startsWith(QLatin1Char('/'))
                           ? topicSuffix
                           : (QStringLiteral("/") + topicSuffix);
        }

        RawBagMessage msg;
        msg.bagIndex = bagIndex;
        msg.topicName = rawTopic;
        msg.typeName = q.value(1).toString();
        msg.timestampNs = q.value(2).toLongLong();
        msg.rawData = q.value(3).toByteArray();
        result.push_back(std::move(msg));
    }

    return result;
}

// ---------------------------------------------------------------------------
// getMaxMessageCount
// ---------------------------------------------------------------------------
int DatabaseWorker::getMaxMessageCount(int bagIndex)
{
    if (m_initialized) {
        const QString bagPrefix = QStringLiteral("/bag%1/").arg(bagIndex);

        QSqlQuery q(m_db);
        q.prepare(QStringLiteral(
            "SELECT MAX(m.msg_index) FROM messages m "
            "JOIN topics t ON t.id = m.topic_id "
            "WHERE t.name LIKE ?"));
        q.addBindValue(bagPrefix + QStringLiteral("%"));

        if (q.exec() && q.next()) {
            int maxIdx = q.value(0).toInt();
            // maxIdx is 0-based, so count = maxIdx + 1
            return maxIdx + 1;
        }
    }

    return m_bagMaxMessageCount.value(bagIndex, 0);
}
