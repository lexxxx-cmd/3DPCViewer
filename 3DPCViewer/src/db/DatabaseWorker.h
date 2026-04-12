#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QHash>
#include <QList>
#include <vector>

#include "io/BagDataTypes.h"

class DatabaseWorker : public QObject
{
    Q_OBJECT

public:
    explicit DatabaseWorker(QObject* parent = nullptr);
    ~DatabaseWorker() override;

    // Load messages at specific index for a bag (0-based msgIndex)
    // Returns all topics' messages at this index
    std::vector<RawBagMessage> loadMessagesAt(int bagIndex, int msgIndex);

    // Get the maximum message count for a bag (for progress bar)
    int getMaxMessageCount(int bagIndex);

public slots:
    // Must be called from the worker's own thread (connect to QThread::started
    // or invoke via QMetaObject::invokeMethod with Qt::QueuedConnection).
    void initDatabase(const QString& dbPath);

    // Receives one CDR-encoded message and buffers it for batch insertion.
    // msgIndex is the sequential index (0-based) within the bag.
    void saveMessage(const RawBagMessage& msg, int msgIndex);

    // Flushes any messages still in the buffer to disk.  Call before the
    // worker thread exits so no data is lost.
    void flushBuffer();

signals:
    // Emitted when messages at a specific index are loaded from DB
    void messagesLoaded(int bagIndex, int msgIndex, const std::vector<RawBagMessage>& messages);

private:
    void commitBatch();
    int  getOrRegisterTopicId(const QString& topicName, const QString& typeName);

    static constexpr int BATCH_SIZE = 500;
    static constexpr char DB_CONN_NAME[] = "db_worker_connection";

    struct PendingMessage {
        int      topicId;
        qint64   timestampNs;
        int      msgIndex;
        QByteArray data;
    };

    QSqlDatabase          m_db;
    bool                  m_initialized{false};
    QHash<QString, int>   m_topicIdCache;
    QList<PendingMessage> m_messageBuffer;
    QHash<int, int>       m_bagMaxMessageCount; // bagIndex -> max msg count
};
