#include "pcviewer.h"
#include "BagWorker.h"
#include <QThread>

PCViewer::PCViewer(QWidget *parent)
    : QMainWindow(parent)
{
    ui = std::make_unique<Ui::PCViewerClass>();
    ui->setupUi(this);

    //rosbag thread
    QThread* workerThread = new QThread(this);
    BagWorker* worker = new BagWorker();
    worker->moveToThread(workerThread);
    connect(workerThread, &QThread::finished, worker, &QObject::deleteLater);
    connect(ui->ControlWidget, &ControlPanelWidget::requestProcBag, worker, &BagWorker::processBag);
    connect(worker, &BagWorker::cloudFrameReady,
        ui->ShowWidget, &VisualAreaWidget::onCloudFrameReady);
    connect(worker, &BagWorker::odomFrameReady,
        ui->ShowWidget, &VisualAreaWidget::onOdomFrameReady);

    //connect(worker, &BagWorker::imageFrameReady,
    //    ui->dataWidget, &DataWidget::onImageFrameReady);
    workerThread->start();

    connect(ui->ControlWidget, &ControlPanelWidget::requestLoadFile, ui->ShowWidget, &VisualAreaWidget::onOpenFileRequested);
    connect(ui->ControlWidget, &ControlPanelWidget::pointSizeChanged, ui->ShowWidget, &VisualAreaWidget::onChangeSizeRequested);
    connect(ui->ControlWidget, &ControlPanelWidget::pointOpacityChanged, ui->ShowWidget, &VisualAreaWidget::onChangeOpacityRequested);
    connect(ui->ControlWidget, &ControlPanelWidget::requestShowNormals, ui->ShowWidget, &VisualAreaWidget::onShowNormalsRequested);

    connect(ui->ShowWidget, &VisualAreaWidget::sendFileSize, ui->ControlWidget, &ControlPanelWidget::onFileSizeUpdated);
    connect(ui->ShowWidget, &VisualAreaWidget::sendPointSize, ui->ControlWidget, &ControlPanelWidget::onPointSizeUpdated);
    //connect(ui->ShowWidget, &VisualAreaWidget::sendFPS, ui->ControlWidget, &ControlPanelWidget::onFPSUpdated);
}

PCViewer::~PCViewer() = default;

