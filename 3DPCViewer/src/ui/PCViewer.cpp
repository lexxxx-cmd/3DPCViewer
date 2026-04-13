#include "ui/PCViewer.h"
#include <QThread>

PCViewer::PCViewer(QWidget* parent) : QMainWindow(parent) {
  ui = std::make_unique<Ui::PCViewerClass>();
  ui->setupUi(this);

  // Input & Event Connections
  connect(ui->ControlWidget, &ControlPanelWidget::requestProcessBag, this,
          &PCViewer::requestProcessBag);
  connect(ui->ControlWidget, &ControlPanelWidget::progressUpdated, this,
          &PCViewer::progressUpdated);

  // Data Updates -> Child Widgets
  connect(this, &PCViewer::requestUpdateFileSize, ui->ControlWidget,
          &ControlPanelWidget::onFileSizeUpdated);
  connect(this, &PCViewer::topicListReady, ui->ControlWidget,
          &ControlPanelWidget::topicListReady);
  connect(this, &PCViewer::messageNumReady, ui->ControlWidget,
          &ControlPanelWidget::messageNumReady);

  // Render Connections
  connect(this, &PCViewer::cloudFrameReady, ui->ShowWidget,
          &VisualAreaWidget::onCloudFrameReady);
  connect(this, &PCViewer::imageFrameReady, ui->ControlWidget,
          &ControlPanelWidget::imageFrameReady);
  connect(this, &PCViewer::odomFrameReady, ui->ShowWidget,
          &VisualAreaWidget::onOdomFrameReady);

  connect(ui->ControlWidget, &ControlPanelWidget::pointSizeChanged, ui->ShowWidget,
          &VisualAreaWidget::onChangeSizeRequested);
  connect(ui->ControlWidget, &ControlPanelWidget::pointOpacityChanged, ui->ShowWidget,
          &VisualAreaWidget::onChangeOpacityRequested);
  connect(ui->ControlWidget, &ControlPanelWidget::bgColorChanged, ui->ShowWidget,
          &VisualAreaWidget::onChangeBgColorRequested);
}

PCViewer::~PCViewer() = default;


