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

    // Must be called from the DB worker thread.
    std::vector<RawBagMessage> loadMessagesByBagIndex(int bagIndex);

public slots:
    // Must be called from the worker's own thread (connect to QThread::started
    // or invoke via QMetaObject::invokeMethod with Qt::QueuedConnection).
    void initDatabase(const QString& dbPath);

    // Receives one CDR-encoded message and buffers it for batch insertion.
    void saveMessage(const RawBagMessage& msg);

    // Flushes any messages still in the buffer to disk.  Call before the
    // worker thread exits so no data is lost.
    void flushBuffer();

private:
    void commitBatch();
    int  getOrRegisterTopicId(const QString& topicName, const QString& typeName);

    static constexpr int BATCH_SIZE = 500;
    static constexpr char DB_CONN_NAME[] = "db_worker_connection";

    struct PendingMessage {
        int      topicId;
        qint64   timestampNs;
        QByteArray data;
    };

    QSqlDatabase          m_db;
    bool                  m_initialized{false};
    QHash<QString, int>   m_topicIdCache;
    QList<PendingMessage> m_messageBuffer;
};
