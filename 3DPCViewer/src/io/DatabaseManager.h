#pragma once

#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>
#include <QByteArray>
#include <QVariantList>
#include <QMap>
#include <QStringList>
#include <memory>
#include "BagDataTypes.h"

class DatabaseManager : public QObject {
    Q_OBJECT

public:
    explicit DatabaseManager(QObject* parent = nullptr);
    ~DatabaseManager();

public slots:
    void initialize(const QString& bag_path);
    void storeMessage(const QString& bag_uuid, const QString& topic_name, 
                      int msg_index, qint64 timestamp, const QByteArray& payload);
    void storeMessageWithOrigin(const QString& bag_uuid, const QString& origin_name, const QString& topic_name, 
                                int msg_index, qint64 timestamp, const QByteArray& payload);
    QString insertBag(const QString& bag_path);
    void insertTopic(const QString& bag_uuid, const QString& topic_name,
        const QString& msg_type);
    void insertTopicWithOrigin(const QString& bag_uuid, const QString& origin_name, const QString& topic_name,
        const QString& msg_type);
    void batchInsertProcessedFrames(const QString& bag_uuid, const QString& origin_name, const QString& topic_name, 
                                    const QString& msg_type, const QVariantList& msg_indices, 
                                    const QVariantList& timestamps, const QVariantList& payloads);
    void setCurrentDataSource(const QString& bag_uuid, const QString& origin_name);
    void setCurrentOrigin(const QString& origin_name);

    public slots:
    // SLAM Stream execution
    bool initSlamStream();
    void fetchNextSlamFrame();
    void stopSlamStream();

    void updateProgress(const int percent);
    void fetchTopicList();
    void close();

signals:
    void initialized(const QString& bag_uuid);
    void messageStored(const QString& topic_name, int msg_index);
    void payloadReady(const QString& topic_name, const int percent, const QByteArray payload);
    void nextSlamFrameReady(const QString& topic, const QByteArray& payload, qint64 timestamp);
    void slamStreamFinished();
    void topicListReady(const TopicTreeData& topics);
    void messageNumReady(int num);
    void errorOccurred(const QString& error_msg);

private:
    QSqlDatabase db;
    QString db_path;
    QString current_bag_uuid;
    QString current_bag_path;
    QString current_origin_name = "raw";
    std::unordered_map<std::string, QString> topic_table_map;

    std::unique_ptr<QSqlQuery> slam_stream_query = nullptr;

    void createMetaTables();
    QString generateUUID();

    void createDynamicTable(const QString& table_name);
    void optimizeDatabase();
};
