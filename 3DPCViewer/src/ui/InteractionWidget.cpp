#include "ui/InteractionWidget.h"
#include <QSlider>
#include <QCheckBox>
#include <QColorDialog>

InteractionWidget::InteractionWidget(QWidget* parent) : QWidget(parent) {
  ui = std::make_unique<Ui::InteractionWidgetClass>();
  ui->setupUi(this);

  ui->HSlider_size->setMaximum(10);
  ui->HSlider_size->setMinimum(1);
  ui->HSlider_size->setValue(1);
  ui->HSlider_size->setSingleStep(1);

  ui->HSlider_opacity->setMaximum(100);
  ui->HSlider_opacity->setMinimum(10);
  ui->HSlider_opacity->setValue(100);
  ui->HSlider_opacity->setSingleStep(10);
  
  ui->HSlider_progress->setMaximum(max_message_num);
  ui->HSlider_progress->setMinimum(0);
  ui->HSlider_progress->setValue(0);
  ui->HSlider_progress->setSingleStep(1);

  ui->pB_backward->setEnabled(false);
  ui->pB_play_pause->setEnabled(false);
  ui->pB_forward->setEnabled(false);

  connect(ui->HSlider_size, &QSlider::valueChanged,
      this, &InteractionWidget::onSizeSliderChanged);

  connect(ui->HSlider_opacity, &QSlider::valueChanged,
      this, &InteractionWidget::onOpacitySliderChanged);

  timer = new QTimer(this);
  timer->setInterval(100);

  connect(timer, &QTimer::timeout, this, [this]() {
    if (cur_message_num < max_message_num) {
      ui->HSlider_progress->setValue(cur_message_num + 1);
    } else {
      timer->stop();
      is_play = false;
      ui->pB_play_pause->setText("Play");
      ui->pB_backward->setEnabled(true);
    }
  });

  connect(ui->HSlider_progress, &QSlider::valueChanged,
      this, &InteractionWidget::onProgressNumChanged);

  connect(ui->pB_backward, &QPushButton::clicked, this, [this]() {
    if (cur_message_num > 0) {
      ui->HSlider_progress->setValue(cur_message_num - 1);
    } else {
      ui->pB_backward->setEnabled(false);
    }
  });

  connect(ui->pB_forward, &QPushButton::clicked, this, [this]() {
    if (cur_message_num < max_message_num) {
      ui->HSlider_progress->setValue(cur_message_num + 1);
    } else {
      ui->pB_forward->setEnabled(false);
    }
  });

  connect(ui->pB_play_pause, &QPushButton::clicked, this, [this]() {
    is_play = !is_play;
    if (is_play) {
      ui->pB_play_pause->setText("Pause");
      ui->pB_backward->setEnabled(false);
      ui->pB_forward->setEnabled(false);
      timer->start();
    } else {
      ui->pB_play_pause->setText("Play");
      ui->pB_backward->setEnabled(true);
      ui->pB_forward->setEnabled(true);
      timer->stop();
    }
  });
  connect(ui->pB_set_bgColor, &QPushButton::clicked, this, [this]() {
    QColor color = QColorDialog::getColor(Qt::black, this, "Background Color Settings");
    emit bgColorChanged(color);
  });

  onSizeSliderChanged(ui->HSlider_size->value());
  onOpacitySliderChanged(ui->HSlider_opacity->value());
}

InteractionWidget::~InteractionWidget() = default;

void InteractionWidget::onSizeSliderChanged(int value) {
  ui->lbl_PSize->setText(QString("%1").arg(value));
  emit pointSizeChanged(value);
}

void InteractionWidget::onOpacitySliderChanged(int value) {
  ui->lbl_Popacity->setText(QString("%1%").arg(value));
  emit pointOpacityChanged(value);
}

void InteractionWidget::onMaxMessageNumSet(int value) {
  max_message_num = value;
  if (max_message_num > 0) {
    ui->pB_play_pause->setEnabled(true);
    ui->pB_forward->setEnabled(true);
  }
  ui->HSlider_progress->setMaximum(max_message_num);
  ui->lbl_ProgressValue->setText(QString("%1/%2").arg(ui->HSlider_progress->value()).arg(max_message_num));
}

void InteractionWidget::onProgressNumChanged(int value) {
  cur_message_num = value;
  if (cur_message_num > 0) {
    ui->pB_backward->setEnabled(true);
  } else {
    ui->pB_backward->setEnabled(false);
  }
  ui->lbl_ProgressValue->setText(QString("%1/%2").arg(cur_message_num).arg(max_message_num));
  emit progressUpdated(cur_message_num);
}

void InteractionWidget::onImageFrameReady(const ImageFrame& frame) {
  if (frame.image.isNull()) return;
  ui->widget_8->setImage(QPixmap::fromImage(frame.image).scaled(
      ui->widget_8->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}
