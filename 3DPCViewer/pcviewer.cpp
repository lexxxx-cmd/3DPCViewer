#include "pcviewer.h"

PCViewer::PCViewer(QWidget *parent)
    : QMainWindow(parent)
{
    ui = std::make_unique<Ui::PCViewerClass>();
    ui->setupUi(this);

    connect(ui->ControlWidget, &ControlPanelWidget::requestLoadFile, ui->ShowWidget, &VisualAreaWidget::onOpenFileRequested);
    connect(ui->ControlWidget, &ControlPanelWidget::pointSizeChanged, ui->ShowWidget, &VisualAreaWidget::onChangeSizeRequested);
    connect(ui->ControlWidget, &ControlPanelWidget::pointOpacityChanged, ui->ShowWidget, &VisualAreaWidget::onChangeOpacityRequested);
    connect(ui->ControlWidget, &ControlPanelWidget::requestShowNormals, ui->ShowWidget, &VisualAreaWidget::onShowNormalsRequested);

    connect(ui->ShowWidget, &VisualAreaWidget::sendFileSize, ui->ControlWidget, &ControlPanelWidget::onFileSizeUpdated);
    connect(ui->ShowWidget, &VisualAreaWidget::sendPointSize, ui->ControlWidget, &ControlPanelWidget::onPointSizeUpdated);
    connect(ui->ShowWidget, &VisualAreaWidget::sendFPS, ui->ControlWidget, &ControlPanelWidget::onFPSUpdated);
}

PCViewer::~PCViewer() = default;

