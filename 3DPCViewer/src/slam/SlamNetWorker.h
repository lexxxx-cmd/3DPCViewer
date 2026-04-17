#pragma once

#include <QObject>
#include <QTimer>
#include <QByteArray>
#include <QList>
#include <QString>
#include <memory>
#include "SlamNetProtocol.h"

namespace slam {

/**
 * @brief ZeroMQ network background worker. 
 * Designed to be moved to a QThread via moveToThread().
 * Abstracts ZMQ communication from Qt event loop using non-blocking polling.
 */
class SlamNetWorker : public QObject {
    Q_OBJECT
public:
    explicit SlamNetWorker(QObject* parent = nullptr);
    ~SlamNetWorker();

public slots:
    /**
     * @brief Initialize ZMQ context and socket. Must be called AFTER moving to thread.
     * @param address ZMQ endpoint address (e.g. "tcp://127.0.0.1:5555")
     */
    void initWorker(const QString& address);

    /**
     * @brief Cleanup and close ZMQ connection
     */
    void cleanupWorker();

    /**
     * @brief Send a command and optional multipart binary data to the SLAM node
     * @param cmd Command header (START, FRAME, FINISH)
     * @param parts List of binary payload parts (e.g. Timestamp, Topic, Blob)
     */
    void sendRequest(slam::net::Command cmd, const QList<QByteArray>& parts = QList<QByteArray>());

signals:
    void workerInitialized();
    void responseReceived(slam::net::Command cmd, const QList<QByteArray>& parts);
    void errorOccurred(const QString& errorMsg);

private slots:
    void onPollTimer();

private:
    std::unique_ptr<zmq::context_t> context_;
    std::unique_ptr<zmq::socket_t> socket_;
    QTimer* pollTimer_{nullptr};
    bool isWaitingForReply_{false};
};

} // namespace slam