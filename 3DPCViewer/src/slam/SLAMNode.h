#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

namespace slam {

class SLAMNode : public QObject {
    Q_OBJECT
public:
    explicit SLAMNode(QObject* parent = nullptr);
    ~SLAMNode();

    /**
     * @brief Start the separated SLAM executable process.
     * @param executablePath Path to the SLAM node executable (e.g., mock_slam_node.exe)
     * @param args Arguments to pass to the process
     * @return true if process started successfully, otherwise false.
     */
    bool start(const QString& executablePath, const QStringList& args = QStringList(), const QString & workingDir = "");

    /**
     * @brief Safely terminate the underlying process.
     */
    void stop();

    /**
     * @brief Check if process is running.
     */
    bool isRunning() const;

signals:
    void outputReceived(const QString& output);
    void errorReceived(const QString& error);
    void nodeFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void nodeStarted();
    void nodeError(QProcess::ProcessError error);

private slots:
    void onReadyReadStandardOutput();
    void onReadyReadStandardError();

private:
    QProcess process_;
};

} // namespace slam
