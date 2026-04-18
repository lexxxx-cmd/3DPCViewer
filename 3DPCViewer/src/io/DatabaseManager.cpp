#include "DatabaseManager.h"
#include <QSqlError>
#include <QUuid>
#include <QDebug>
#include <QFileInfo>

DatabaseManager::DatabaseManager(QObject* parent) : QObject(parent) {
    // Thread management is handled by DataService
    // Do not create own thread here to avoid double thread movement
}

DatabaseManager::~DatabaseManager() {
    if (db.isOpen()) {
        db.close();
    }
    // Thread lifecycle is managed by DataService
}

void DatabaseManager::initialize(const QString& bag_path) {
    try {
        QFileInfo bag_info(bag_path);
        QString bag_dir = bag_info.absolutePath();
        db_path = bag_dir + "/temp.db";
        
        db = QSqlDatabase::addDatabase("QSQLITE", "bag_connection_" + QUuid::createUuid().toString());
        db.setDatabaseName(db_path);
        
        if (!db.open()) {
            throw std::runtime_error("Failed to open database: " + db.lastError().text().toStdString());
        }
        
        optimizeDatabase();
        createMetaTables();

        current_bag_path = bag_path;

        // current_bag_uuid will be set in insertTopicWithOrigin
        emit initialized("");
    } catch (const std::exception& e) {
        emit errorOccurred(QString::fromStdString(e.what()));
    }
}

void DatabaseManager::createMetaTables() {
    QSqlQuery query(db);
    
    QString create_bags_table = 
        "CREATE TABLE IF NOT EXISTS bags ("
        "bag_uuid TEXT PRIMARY KEY, "
        "file_path TEXT UNIQUE NOT NULL)";
    
    if (!query.exec(create_bags_table)) {
        throw std::runtime_error("Failed to create bags table: " + query.lastError().text().toStdString());
    }
    
    QString create_topics_table = 
        "CREATE TABLE IF NOT EXISTS origin ("
        "bag_uuid TEXT NOT NULL, "
        "origin_name TEXT NOT NULL, "
        "topic_name TEXT NOT NULL, "
        "msg_type TEXT, "
        "dynamic_table_name TEXT NOT NULL, "
        "PRIMARY KEY(bag_uuid, origin_name, topic_name), "
        "FOREIGN KEY(bag_uuid) REFERENCES bags(bag_uuid))";

    if (!query.exec(create_topics_table)) {
        throw std::runtime_error("Failed to create topics table: " + query.lastError().text().toStdString());
    }
}

QString DatabaseManager::insertBag(const QString& bag_path) {
    QString bag_uuid = generateUUID();
    
    QSqlQuery query(db);
    query.prepare("INSERT INTO bags (bag_uuid, file_path) VALUES (?, ?)");
    query.addBindValue(bag_uuid);
    query.addBindValue(bag_path);
    
    if (!query.exec()) {
        throw std::runtime_error("Failed to insert bag: " + query.lastError().text().toStdString());
    }
    
    return bag_uuid;
}

QString DatabaseManager::generateUUID() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

void DatabaseManager::storeMessage(const QString& bag_uuid, const QString& topic_name, 
                                    int msg_index, qint64 timestamp, const QByteArray& payload) {
    storeMessageWithOrigin(bag_uuid, "raw", topic_name, msg_index, timestamp, payload);
}

void DatabaseManager::storeMessageWithOrigin(const QString& bag_uuid, const QString& origin_name, const QString& topic_name, 
                                             int msg_index, qint64 timestamp, const QByteArray& payload) {
    try {
        QString table_name = QString("data_o_%1_b_%2_t_%3")
            .arg(QString::number(qHash(origin_name), 16).mid(0, 4))
            .arg(bag_uuid.mid(0, 8))
            .arg(QString::number(qHash(topic_name), 16).mid(0, 8));
        
        QSqlQuery query(db);
        QString insert_sql = QString("INSERT INTO %1 (msg_index, timestamp, data) VALUES (?, ?, ?)")
                                .arg(table_name);
        query.prepare(insert_sql);
        query.addBindValue(msg_index);
        query.addBindValue(timestamp);
        query.addBindValue(payload);
        
        if (!query.exec()) {
            throw std::runtime_error("Failed to insert message: " + query.lastError().text().toStdString());
        }
        
        emit messageStored(topic_name, msg_index);
    } catch (const std::exception& e) {
        emit errorOccurred(QString::fromStdString(e.what()));
    }
}

void DatabaseManager::insertTopic(const QString& bag_uuid, const QString& topic_name, 
                                   const QString& msg_type) {
    insertTopicWithOrigin(bag_uuid, "raw", topic_name, msg_type);
}

void DatabaseManager::insertTopicWithOrigin(const QString& bag_uuid, const QString& origin_name, 
                                             const QString& topic_name, const QString& msg_type) {
    current_bag_uuid = bag_uuid;

    QSqlQuery check_bag(db);
    check_bag.prepare("SELECT 1 FROM bags WHERE bag_uuid = ?");
    check_bag.addBindValue(bag_uuid);
    if (check_bag.exec() && !check_bag.next()) {
        QSqlQuery insert_bag(db);
        insert_bag.prepare("INSERT OR REPLACE INTO bags (bag_uuid, file_path) VALUES (?, ?)");
        insert_bag.addBindValue(bag_uuid);
        insert_bag.addBindValue(current_bag_path);
        insert_bag.exec();
    }

    QString table_name = QString("data_o_%1_b_%2_t_%3")
                            .arg(QString::number(qHash(origin_name), 16).mid(0, 4))
                            .arg(bag_uuid.mid(0, 8))
                            .arg(QString::number(qHash(topic_name), 16).mid(0, 8));

    createDynamicTable(table_name);

    QSqlQuery query(db);
    query.prepare("INSERT OR IGNORE INTO origin (bag_uuid, origin_name, topic_name, msg_type, dynamic_table_name) "
                  "VALUES (?, ?, ?, ?, ?)");
    query.addBindValue(bag_uuid);
    query.addBindValue(origin_name);
    query.addBindValue(topic_name);
    query.addBindValue(msg_type);
    query.addBindValue(table_name);

    if (!query.exec()) {
        throw std::runtime_error("Failed to insert topic: " + query.lastError().text().toStdString());
    }
}

void DatabaseManager::setCurrentDataSource(const QString& bag_uuid, const QString& origin_name) {
    current_bag_uuid = bag_uuid;
    current_origin_name = origin_name;
    qDebug() << "Database focal point switched -> UUID:" << current_bag_uuid << "| Origin:" << current_origin_name;

    // Compute new max message count for the current focal point
    int max_msg_count = 0;
    QSqlQuery query(db);
    query.prepare("SELECT dynamic_table_name FROM origin WHERE bag_uuid = ? AND origin_name = ?");
    query.addBindValue(current_bag_uuid);
    query.addBindValue(current_origin_name);

    if (query.exec()) {
        while (query.next()) {
            QString table_name = query.value(0).toString();
            QSqlQuery count_query(db);
            count_query.prepare(QString("SELECT MAX(msg_index) FROM %1").arg(table_name));
            if (count_query.exec() && count_query.next()) {
                int count = count_query.value(0).toInt() + 1; // 0-based index
                if (count > max_msg_count) {
                    max_msg_count = count;
                }
            }
        }
    }

    // Notify the UI
    emit messageNumReady(max_msg_count);
}

void DatabaseManager::setCurrentOrigin(const QString& origin_name) {
    current_origin_name = origin_name;
}

void DatabaseManager::createDynamicTable(const QString& table_name) {
    QSqlQuery query(db);
    QString create_sql = QString(
        "CREATE TABLE IF NOT EXISTS %1 ("
        "msg_index INTEGER PRIMARY KEY, "
        "timestamp INTEGER NOT NULL, "
        "data BLOB NOT NULL)"
    ).arg(table_name);
    
    if (!query.exec(create_sql)) {
        throw std::runtime_error("Failed to create dynamic table: " + query.lastError().text().toStdString());
    }
}

void DatabaseManager::optimizeDatabase() {
    QSqlQuery query(db);
    query.exec("PRAGMA synchronous = OFF;");
    query.exec("PRAGMA journal_mode = MEMORY;");
    query.exec("PRAGMA temp_store = MEMORY;");
    query.exec("PRAGMA cache_size = -200000;");
}

void DatabaseManager::close() {
    if (db.isOpen()) {
        db.close();
    }
}

void DatabaseManager::batchInsertProcessedFrames(const QString& bag_uuid, const QString& origin_name, const QString& topic_name, 
                                                 const QString& msg_type, const QVariantList& msg_indices, 
                                                 const QVariantList& timestamps, const QVariantList& payloads) {
    try {
        if (msg_indices.size() != timestamps.size() || timestamps.size() != payloads.size()) {
            throw std::runtime_error("batchInsertProcessedFrames: List sizes do not match.");
        }

        // 1. Ensure topic entry exists in origin
        insertTopicWithOrigin(bag_uuid, origin_name, topic_name, msg_type);

        QString table_name = QString("data_o_%1_b_%2_t_%3")
                                .arg(QString::number(qHash(origin_name), 16).mid(0, 4))
                                .arg(bag_uuid.mid(0, 8))
                                .arg(QString::number(qHash(topic_name), 16).mid(0, 8));

        // 2. Perform batched transaction inserts
        db.transaction();
        QSqlQuery query(db);
        QString insert_sql = QString("INSERT INTO %1 (msg_index, timestamp, data) VALUES (?, ?, ?)")
                                .arg(table_name);
        query.prepare(insert_sql);

        query.addBindValue(msg_indices);
        query.addBindValue(timestamps);
        query.addBindValue(payloads);

        if (!query.execBatch()) {
            throw std::runtime_error("Failed to execute batch insert: " + query.lastError().text().toStdString());
        }
        db.commit();

    } catch (const std::exception& e) {
        db.rollback();
        emit errorOccurred(QString::fromStdString(e.what()));
    }
}

void DatabaseManager::updateProgress(const int percent) {
    try {
        QSqlQuery topic_query(db);
        topic_query.prepare("SELECT topic_name, dynamic_table_name FROM origin WHERE bag_uuid = ? AND origin_name = ?");
        topic_query.addBindValue(current_bag_uuid);
        topic_query.addBindValue(current_origin_name);

        if (!topic_query.exec()) {
            throw std::runtime_error("Failed to query topics: " + topic_query.lastError().text().toStdString());
        }

        while (topic_query.next()) {
            const QString topic_name = topic_query.value(0).toString();
            const QString table_name = topic_query.value(1).toString();

            QSqlQuery payload_query(db);
            const QString payload_sql = QString("SELECT data FROM %1 WHERE msg_index = ?").arg(table_name);
            payload_query.prepare(payload_sql);
            payload_query.addBindValue(percent);

            if (!payload_query.exec()) {
                throw std::runtime_error("Failed to query payload: " + payload_query.lastError().text().toStdString());
            }

            if (payload_query.next()) {
                emit payloadReady(topic_name, percent, payload_query.value(0).toByteArray());
            }
        }
    } catch (const std::exception& e) {
        emit errorOccurred(QString::fromStdString(e.what()));
    }
}

void DatabaseManager::fetchTopicList() {
    try {
        QSqlQuery query(db);
        query.prepare("SELECT b.bag_uuid, b.file_path, o.origin_name, o.topic_name "
                      "FROM origin o "
                      "JOIN bags b ON o.bag_uuid = b.bag_uuid "
                      "ORDER BY b.file_path, o.origin_name");

        if (!query.exec()) {
            throw std::runtime_error("Failed to fetch topics: " + query.lastError().text().toStdString());
        }

        TopicTreeData all_topics;
        while (query.next()) {
            QString bag_uuid = query.value(0).toString();
            QString file_path = query.value(1).toString();
            QString bag_name = QFileInfo(file_path).fileName();
            QString origin_name = query.value(2).toString();
            QString topic_name = query.value(3).toString();

            QString compound_key = bag_uuid + "|" + bag_name;

            all_topics[compound_key][origin_name].append(topic_name);
        }

        emit topicListReady(all_topics);
    } catch (const std::exception& e) {
        emit errorOccurred(QString::fromStdString(e.what()));
    }
}