#include "SlamNetWorker.h"
#include <QDebug>
#include <QThread>

namespace slam {

SlamNetWorker::SlamNetWorker(QObject* parent)
    : QObject(parent)
{
}

SlamNetWorker::~SlamNetWorker() {
    cleanupWorker();
}

void SlamNetWorker::initWorker(const QString& address) {
    try {
        qDebug() << "[SlamNetWorker] Initializing on Thread:" << QThread::currentThreadId();

        // 1 for thread context is usually enough for single client
        context_ = std::make_unique<zmq::context_t>(1);
        socket_ = std::make_unique<zmq::socket_t>(*context_, zmq::socket_type::req);

        // Ensure no lingering blocking on exit
        socket_->set(zmq::sockopt::linger, 0); 

        qDebug() << "[SlamNetWorker] Connecting to ZMQ endpoint:" << address;
        socket_->connect(address.toStdString());

        pollTimer_ = new QTimer(this);
        connect(pollTimer_, &QTimer::timeout, this, &SlamNetWorker::onPollTimer);
        pollTimer_->start(5); // poll every 5ms

        isWaitingForReply_ = false;
        emit workerInitialized();
    } catch (const zmq::error_t& e) {
        emit errorOccurred(QString("ZMQ Init Error: %1").arg(e.what()));
    }
}

void SlamNetWorker::cleanupWorker() {
    qDebug() << "[SlamNetWorker] Cleaning up ZMQ context on Thread:" << QThread::currentThreadId();
    if (pollTimer_) {
        pollTimer_->stop();
        pollTimer_->deleteLater();
        pollTimer_ = nullptr;
    }
    if (socket_) {
        socket_->close();
        socket_.reset();
    }
    if (context_) {
        context_->close();
        context_.reset();
    }
    isWaitingForReply_ = false;
}

void SlamNetWorker::sendRequest(slam::net::Command cmd, const QList<QByteArray>& parts) {
    if (!socket_) {
        emit errorOccurred("[SlamNetWorker] Socket not initialized.");
        return;
    }

    // Check if we are still waiting for a reply before sending new request 
    // REQ-REP pattern demands strictly alternating send/receive.
    if (isWaitingForReply_) {
        qWarning() << "[SlamNetWorker] Cannot send request. Still waiting for previous reply.";
        return;
    }

    try {
        zmq::send_flags flags = parts.isEmpty() ? zmq::send_flags::none : zmq::send_flags::sndmore;
        if (!net::sendCommand(*socket_, cmd, flags)) {
             emit errorOccurred("[SlamNetWorker] Failed to send command.");
             return;
        }

        // Send all multi-parts sequentially
        for (int i = 0; i < parts.size(); ++i) {
            zmq::send_flags pFlags = (i == parts.size() - 1) ? zmq::send_flags::none : zmq::send_flags::sndmore;
            zmq::message_t msg(parts[i].size());
            if (parts[i].size() > 0) {
                std::memcpy(msg.data(), parts[i].constData(), parts[i].size());
            }
            auto res = socket_->send(msg, pFlags);
            if (!res.has_value()) {
                 emit errorOccurred(QString("[SlamNetWorker] Failed to send part %1.").arg(i));
                 return;
            }
        }

        isWaitingForReply_ = true;
    } catch (const zmq::error_t& e) {
        emit errorOccurred(QString("ZMQ Send Error: %1").arg(e.what()));
    }
}

void SlamNetWorker::onPollTimer() {
    // 限流打印：每 1 秒（5ms * 200）打印一次心跳
    static int tick = 0;
    if (tick++ % 200 == 0) {
        qDebug() << "[SlamNetWorker] Timer ticking. socket=" << (socket_ != nullptr) 
                 << " waiting=" << isWaitingForReply_;
    }
    if (!socket_ || !isWaitingForReply_) {
        return;
    }

    try {
        zmq::message_t headerMsg;
        // Try to receive response (non-blocking)
        auto res = socket_->recv(headerMsg, zmq::recv_flags::dontwait);

        if (!res.has_value()) return; // No data yet, wait for next poll

        // --- As long as we receive something, socket has received a response ---
        isWaitingForReply_ = false; // Reset state, avoid duplicate status reports in next poll

        if (headerMsg.size() != sizeof(net::Command)) {
            emit errorOccurred("Protocol Mismatch: Expected 1 byte, got " + QString::number(headerMsg.size()));
            // Clear remaining frames to avoid data corruption
            while (socket_->get(zmq::sockopt::rcvmore)) {
                zmq::message_t tmp; socket_->recv(tmp);
            }
            return;
        }

        net::Command replyCmd = *static_cast<const net::Command*>(headerMsg.data());


        // We successfully received the command header, pull all remaining parts
        QList<QByteArray> parts;
        int more = socket_->get(zmq::sockopt::rcvmore);
        while (more) {
            zmq::message_t partMsg;
            // Next parts shouldn't block, so standard receive is fine
            auto res = socket_->recv(partMsg, zmq::recv_flags::none); 
            if (res.has_value()) {
                parts.append(QByteArray(static_cast<const char*>(partMsg.data()), partMsg.size()));
            }
            more = socket_->get(zmq::sockopt::rcvmore);
        }

        // State reset: we are allowed to send the next message
        isWaitingForReply_ = false;
        qDebug() << "[SlamNetWorker] Actually emitting response! CMD:" << static_cast<int>(replyCmd);
        emit responseReceived(replyCmd, parts);

    } catch (const zmq::error_t& e) {
        isWaitingForReply_ = false; // Reset state even on exception
        emit errorOccurred(QString("ZMQ Receive Error: %1").arg(e.what()));
    }
}

} // namespace slam