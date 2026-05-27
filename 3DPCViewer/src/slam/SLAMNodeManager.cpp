#include "SLAMNodeManager.h"
#include <QDebug>
#include <QMetaObject>

namespace slam {

SLAMNodeManager::SLAMNodeManager(QObject* parent)
    : QObject(parent), activeNode_(std::make_unique<SLAMNode>())
{
    // Ensure node errors and outputs are properly forwarded to users/UI
    connect(activeNode_.get(), &SLAMNode::nodeStarted, this, &SLAMNodeManager::nodeStarted);
    connect(activeNode_.get(), static_cast<void(SLAMNode::*)(int, QProcess::ExitStatus)>(&SLAMNode::nodeFinished),
            this, &SLAMNodeManager::nodeFinished);
    connect(activeNode_.get(), &SLAMNode::outputReceived, this, &SLAMNodeManager::nodeOutputReceived);
    connect(activeNode_.get(), &SLAMNode::errorReceived, this, &SLAMNodeManager::nodeErrorReceived);

    // Prepare Network Worker and Thread
    netThread_ = new QThread(this);
    netWorker_ = new SlamNetWorker();
    netWorker_->moveToThread(netThread_);

    // Connect Worker signals/slots
    connect(netWorker_, &SlamNetWorker::workerInitialized, this, &SLAMNodeManager::onWorkerInitialized);
    connect(netWorker_, &SlamNetWorker::responseReceived, this, &SLAMNodeManager::onWorkerResponse);
    connect(netWorker_, &SlamNetWorker::errorOccurred, this, &SLAMNodeManager::onWorkerError);

    // Ensure automatic cleanup of worker on thread exit
    connect(netThread_, &QThread::finished, netWorker_, &QObject::deleteLater);

    // Start the thread loop immediately (the Worker itself stays dormant until initWorker is called)
    netThread_->start();
}

SLAMNodeManager::~SLAMNodeManager() {
    stopAlgorithm();
    if (netThread_) {
        netThread_->quit();
        netThread_->wait();
    }
}

bool SLAMNodeManager::startAlgorithm(const QString& executablePath, const QStringList& args, const QString& zmqAddress, const QString& workingDir) {
    if (activeNode_->isRunning()) {
        emit managerError("[SLAMNodeManager] Attempting to start, but another SLAM node is currently running.");
        return false;
    }

    qDebug() << "[SLAMNodeManager] Initializing SLAM Algorithm:" << executablePath;

    // Asynchronously initialize the Worker that is sitting on another thread
    QMetaObject::invokeMethod(netWorker_, "initWorker", Qt::QueuedConnection, Q_ARG(QString, zmqAddress));
    return true;
}

void SLAMNodeManager::stopAlgorithm() {
    if (activeNode_->isRunning()) {
        qDebug() << "[SLAMNodeManager] Stopping active SLAM Algorithm node...";
        activeNode_->stop();
    }

    if (netThread_ && netThread_->isRunning()) {
        // Run cleanup on worker asynchronously
        QMetaObject::invokeMethod(netWorker_, "cleanupWorker", Qt::QueuedConnection);
    }
}

bool SLAMNodeManager::isRunning() const {
    return activeNode_->isRunning();
}

void SLAMNodeManager::sendStartCommand(const QList<QByteArray>& payload) {
    QMetaObject::invokeMethod(netWorker_, "sendRequest", Qt::QueuedConnection,
                              Q_ARG(slam::net::Command, slam::net::Command::CMD_START),
                              Q_ARG(QList<QByteArray>, payload));
}

void SLAMNodeManager::sendFrameCommand(const QList<QByteArray>& payload) {
    QMetaObject::invokeMethod(netWorker_, "sendRequest", Qt::QueuedConnection,
                              Q_ARG(slam::net::Command, slam::net::Command::CMD_FRAME),
                              Q_ARG(QList<QByteArray>, payload));
}

void SLAMNodeManager::sendFinishCommand(const QList<QByteArray>& payload) {
    QMetaObject::invokeMethod(netWorker_, "sendRequest", Qt::QueuedConnection,
                              Q_ARG(slam::net::Command, slam::net::Command::CMD_FINISH),
                              Q_ARG(QList<QByteArray>, payload));
}

void SLAMNodeManager::onWorkerInitialized() {
    qDebug() << "[SLAMNodeManager] ZMQ Network Worker Initialized.";
    emit networkInitialized();
}

void SLAMNodeManager::onWorkerResponse(slam::net::Command cmd, const QList<QByteArray>& parts) {
    // Simply route up to the DataService layer
    emit slamResponseReceived(cmd, parts);
}

void SLAMNodeManager::onWorkerError(const QString& errorMsg) {
    qWarning() << "[SLAMNodeManager] Worker Error:" << errorMsg;
    emit managerError(errorMsg);
}

} // namespace slam