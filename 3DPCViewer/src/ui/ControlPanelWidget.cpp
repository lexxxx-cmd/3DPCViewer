#include "ControlPanelWidget.h"

ControlPanelWidget::ControlPanelWidget(QWidget *parent)
    : QWidget(parent)
{
    ui = std::make_unique<Ui::ControlPanelWidgetClass>();
    ui->setupUi(this);

    connect(ui->DataW, &DataWidget::requestProcBag, this, [this](const QString& path) {
        emit requestProcBag(path);
    });
    connect(this, &ControlPanelWidget::topicListReady, ui->StatusW, &StatusWidget::onUpdateTopicList);
    connect(this, &ControlPanelWidget::messageNumReady, ui->InterWidget, &InteractionWidget::onMaxmessageNumSet);
    connect(this, &ControlPanelWidget::onImageFrameReady, ui->InterWidget, &InteractionWidget::onImageFrameReady);

    connect(ui->InterWidget, &InteractionWidget::progressUpdated, this, [this](const int value) {
        emit progressUpdated(value);
    });

    connect(ui->InterWidget, &InteractionWidget::pointSizeChanged, this, [this](const int& value) {
        emit pointSizeChanged(value);
    });
    connect(ui->InterWidget, &InteractionWidget::pointOpacityChanged, this, [this](const int& value) {
        emit pointOpacityChanged(value);
    });
    connect(ui->InterWidget, &InteractionWidget::bgColorChanged, this, [this](const QColor& color) {
        emit bgColorChanged(color);
    });
}

ControlPanelWidget::~ControlPanelWidget() = default;

void ControlPanelWidget::onFileSizeUpdated(const int& size) {
    emit requestUpdateFileSize(size);
}

void ControlPanelWidget::onPointSizeUpdated(const int& num) {
    emit requestUpdatePointSize(num);
}
