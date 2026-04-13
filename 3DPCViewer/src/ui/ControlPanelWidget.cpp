#include "ControlPanelWidget.h"

ControlPanelWidget::ControlPanelWidget(QWidget* parent) : QWidget(parent) {
  ui = std::make_unique<Ui::ControlPanelWidgetClass>();
  ui->setupUi(this);
  
  connect(ui->DataW, &DataWidget::requestProcessBag, this,
          &ControlPanelWidget::requestProcessBag);
  connect(this, &ControlPanelWidget::topicListReady, ui->StatusW,
          &StatusWidget::onUpdateTopicList);
  connect(this, &ControlPanelWidget::messageNumReady, ui->InterWidget,
          &InteractionWidget::onMaxMessageNumSet);
  connect(this, &ControlPanelWidget::imageFrameReady, ui->InterWidget,
          &InteractionWidget::onImageFrameReady);

  connect(ui->InterWidget, &InteractionWidget::progressUpdated, this,
          &ControlPanelWidget::progressUpdated);
  connect(ui->InterWidget, &InteractionWidget::pointSizeChanged, this,
          &ControlPanelWidget::pointSizeChanged);
  connect(ui->InterWidget, &InteractionWidget::pointOpacityChanged, this,
          &ControlPanelWidget::pointOpacityChanged);
  connect(ui->InterWidget, &InteractionWidget::bgColorChanged, this,
          &ControlPanelWidget::bgColorChanged);
}

void ControlPanelWidget::onFileSizeUpdated(const int& size) {
  emit requestUpdateFileSize(size);
}

void ControlPanelWidget::onPointSizeUpdated(const int& num) {
  emit requestUpdatePointSize(num);
}
