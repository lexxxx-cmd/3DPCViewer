#pragma once

#include <QObject>
#include <QThread>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QList>
#include <QByteArray>
#include <memory>

#include "SLAMNode.h"
#include "SlamNetWorker.h"
#include "SlamNetProtocol.h"

namespace slam {

/**
 * @brief Central manager for SLAM processes and ZMQ network communication.
 * Coordinates between the external SLAM executable and the background ZMQ worker thread.
 */
class SLAMNodeManager : public QObject {
    Q_OBJECT
public:
    explicit SLAMNodeManager(QObject* parent = nullptr);
    ~SLAMNodeManager();

    /**
     * @brief Starts the specified SLAM algorithm process and initializes network.
     * @param executablePath Path to the SLAM node executable
     * @param args Command line arguments for the process
     * @param zmqAddress Address for the ZMQ socket (default: "tcp://127.0.0.1:5555")
     */
    bool startAlgorithm(const QString& executablePath, const QStringList& args = QStringList(), const QString& zmqAddress = "tcp://127.0.0.1:5555");

    /**
     * @brief Stops the current SLAM algorithm and cleans up network.
     */
    void stopAlgorithm();

    // Protocol wrappers for semantic usage
    void sendStartCommand(const QList<QByteArray>& payload = QList<QByteArray>());
    void sendFrameCommand(const QList<QByteArray>& payload);
    void sendFinishCommand(const QList<QByteArray>& payload = QList<QByteArray>());

    bool isRunning() const;

signals:
    // Process related
    void nodeStarted();
    void nodeFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void nodeOutputReceived(const QString& output);
    void nodeErrorReceived(const QString& error);

    // Network related
    void networkInitialized();
    void slamResponseReceived(slam::net::Command cmd, const QList<QByteArray>& parts);
    void managerError(const QString& errorMsg);

private slots:
    void onWorkerInitialized();
    void onWorkerResponse(slam::net::Command cmd, const QList<QByteArray>& parts);
    void onWorkerError(const QString& errorMsg);

private:
    std::unique_ptr<SLAMNode> activeNode_;

    QThread* netThread_{nullptr};
    SlamNetWorker* netWorker_{nullptr};
};

} // namespace slam