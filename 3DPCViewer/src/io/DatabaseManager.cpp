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
        
        current_bag_uuid = insertBag(bag_path);
        
        emit initialized(current_bag_uuid);
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
        "CREATE TABLE IF NOT EXISTS topics ("
        "bag_uuid TEXT NOT NULL, "
        "topic_name TEXT NOT NULL, "
        "msg_type TEXT, "
        "dynamic_table_name TEXT NOT NULL, "
        "PRIMARY KEY(bag_uuid, topic_name), "
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
    try {
        QString table_name = QString("data_b_%1_t_%2")
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
    current_bag_uuid = bag_uuid;

    QString table_name = QString("data_b_%1_t_%2")
                            .arg(bag_uuid.mid(0, 8))
                            .arg(QString::number(qHash(topic_name), 16).mid(0, 8));
    
    createDynamicTable(table_name);
    
    QSqlQuery query(db);
    query.prepare("INSERT INTO topics (bag_uuid, topic_name, msg_type, dynamic_table_name) "
                  "VALUES (?, ?, ?, ?)");
    query.addBindValue(bag_uuid);
    query.addBindValue(topic_name);
    query.addBindValue(msg_type);
    query.addBindValue(table_name);
    
    if (!query.exec()) {
        throw std::runtime_error("Failed to insert topic: " + query.lastError().text().toStdString());
    }
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

void DatabaseManager::updateProgress(const int percent) {
    try {
        QSqlQuery topic_query(db);
        topic_query.prepare("SELECT topic_name, dynamic_table_name FROM topics WHERE bag_uuid = ?");
        topic_query.addBindValue(current_bag_uuid);

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