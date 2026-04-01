#include "pcviewer.h"
#include <QThread>

PCViewer::PCViewer(QWidget *parent)
    : QMainWindow(parent)
{
    ui = std::make_unique<Ui::PCViewerClass>();
    ui->setupUi(this);

    connect(ui->ControlWidget, &ControlPanelWidget::requestProcBag, this, [this](const QString& path) {
        emit requestProcBag(path);
        });
    connect(this, &PCViewer::cloudFrameReady, ui->ShowWidget, &VisualAreaWidget::onCloudFrameReady);
    connect(this, &PCViewer::imageFrameReady, ui->ShowWidget, &VisualAreaWidget::onImageFrameReady);
    connect(this, &PCViewer::odomFrameReady, ui->ShowWidget, &VisualAreaWidget::onOdomFrameReady);

    connect(ui->ControlWidget, &ControlPanelWidget::pointSizeChanged, ui->ShowWidget, &VisualAreaWidget::onChangeSizeRequested);
    connect(ui->ControlWidget, &ControlPanelWidget::pointOpacityChanged, ui->ShowWidget, &VisualAreaWidget::onChangeOpacityRequested);

}

PCViewer::~PCViewer() = default;

