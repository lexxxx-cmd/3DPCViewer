#include "ui/PCViewer.h"
#include <QThread>

PCViewer::PCViewer(QWidget *parent)
    : QMainWindow(parent)
{
    ui = std::make_unique<Ui::PCViewerClass>();
    ui->setupUi(this);

    connect(ui->ControlWidget, &ControlPanelWidget::requestProcBag, this, [this](const QString& path) {
        emit requestProcBag(path);
        });
    connect(this, &PCViewer::requestUpdateFileSize, ui->ControlWidget, &ControlPanelWidget::onFileSizeUpdated);
    connect(this, &PCViewer::cloudFrameReady, ui->ShowWidget, &VisualAreaWidget::onCloudFrameReady);
    connect(this, &PCViewer::imageFrameReady, ui->ControlWidget, &ControlPanelWidget::onImageFrameReady);
    connect(this, &PCViewer::odomFrameReady, ui->ShowWidget, &VisualAreaWidget::onOdomFrameReady);
    
    connect(this, &PCViewer::topicListReady, ui->ControlWidget, &ControlPanelWidget::topicListReady);
    connect(this, &PCViewer::messageNumReady, ui->ControlWidget, &ControlPanelWidget::messageNumReady);
    connect(ui->ControlWidget, &ControlPanelWidget::progressUpdated, this, &PCViewer::progressUpdated);

    connect(ui->ControlWidget, &ControlPanelWidget::pointSizeChanged, ui->ShowWidget, &VisualAreaWidget::onChangeSizeRequested);
    connect(ui->ControlWidget, &ControlPanelWidget::pointOpacityChanged, ui->ShowWidget, &VisualAreaWidget::onChangeOpacityRequested);

}

PCViewer::~PCViewer() = default;

