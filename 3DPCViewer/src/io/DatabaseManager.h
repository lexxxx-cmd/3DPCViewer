#pragma once

#include <QObject>
#include <QThread>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>
#include <QByteArray>
#include <unordered_map>

class DatabaseManager : public QObject {
    Q_OBJECT

public:
    explicit DatabaseManager(QObject* parent = nullptr);
    ~DatabaseManager();

public slots:
    void initialize(const QString& bag_path);
    void storeMessage(const QString& bag_uuid, const QString& topic_name, 
                      int msg_index, qint64 timestamp, const QByteArray& payload);
    QString insertBag(const QString& bag_path);
    void insertTopic(const QString& bag_uuid, const QString& topic_name,
        const QString& msg_type);
    void updateProgress(const int percent);
    void close();

signals:
    void initialized(const QString& bag_uuid);
    void messageStored(const QString& topic_name, int msg_index);
    void payloadReady(const QString& topic_name, const int percent, const QByteArray payload);
    void errorOccurred(const QString& error_msg);

private:
    QThread* worker_thread;
    QSqlDatabase db;
    QString db_path;
    QString current_bag_uuid;
    std::unordered_map<std::string, QString> topic_table_map;

    void createMetaTables();
    QString generateUUID();

    void createDynamicTable(const QString& table_name);
    void optimizeDatabase();
};
