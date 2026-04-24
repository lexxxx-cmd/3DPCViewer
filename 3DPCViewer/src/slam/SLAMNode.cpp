#include "SLAMNode.h"
#include <QDebug>
#include <QFileInfo>

namespace slam {

SLAMNode::SLAMNode(QObject* parent)
    : QObject(parent)
{
    connect(&process_, &QProcess::readyReadStandardOutput, this, &SLAMNode::onReadyReadStandardOutput);
    connect(&process_, &QProcess::readyReadStandardError, this, &SLAMNode::onReadyReadStandardError);
    connect(&process_, &QProcess::started, this, &SLAMNode::nodeStarted);
    connect(&process_, static_cast<void(QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished), 
            this, &SLAMNode::nodeFinished);
    connect(&process_, &QProcess::errorOccurred, this, &SLAMNode::nodeError);
    // 在构造函数里连接
    connect(&process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        qDebug() << "[SLAMNode] Critical Error Code:" << error;
        });
}

SLAMNode::~SLAMNode() {
    stop();
}

bool SLAMNode::start(const QString& executablePath, const QStringList& args, const QString& workingDir) {
    if (!workingDir.isEmpty()) {
        process_.setWorkingDirectory(workingDir);
    } else {
        QFileInfo exeInfo(executablePath);
        process_.setWorkingDirectory(exeInfo.absolutePath());
    }
    if (isRunning()) {
        qWarning() << "[SLAMNode] Process is already running. You must stop it first.";
        return false;
    }

    qDebug() << "[SLAMNode] Starting external SLAM process:" << executablePath << args;
    process_.start(executablePath, args);
    return process_.waitForStarted(3000); // 3 seconds timeout to wait for it to start
}

void SLAMNode::stop() {
    if (isRunning()) {
        qDebug() << "[SLAMNode] Requesting termination of SLAM node...";
        process_.terminate();
        if (!process_.waitForFinished(3000)) { // wait up to 3 seconds for graceful shutdown
            qWarning() << "[SLAMNode] Graceful termination failed. Killing process...";
            process_.kill();
            process_.waitForFinished();
        }
        qDebug() << "[SLAMNode] Process stopped successfully.";
    }
}

bool SLAMNode::isRunning() const {
    return process_.state() == QProcess::Running;
}

void SLAMNode::onReadyReadStandardOutput() {
    QByteArray data = process_.readAllStandardOutput();
    // Assuming local 8 bit depending on user platform for normal stdout
    QString output = QString::fromLocal8Bit(data);
    emit outputReceived(output);
}

void SLAMNode::onReadyReadStandardError() {
    QByteArray data = process_.readAllStandardError();
    QString errorMsg = QString::fromLocal8Bit(data);
    emit errorReceived(errorMsg);
}

} // namespace slam