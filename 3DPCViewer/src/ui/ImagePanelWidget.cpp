#include "ImagePanelWidget.h"
#include "ui_ImagePanelWidget.h"

ImagePanelWidget::ImagePanelWidget(QWidget* parent) : QWidget(parent), ui(new Ui::ImagePanelWidget()) {
  ui->setupUi(this);
  ui->contentFrame->setMaximumHeight(0);

  animation = new QPropertyAnimation(ui->contentFrame, "maximumHeight");
  animation->setDuration(300);
  animation->setEasingCurve(QEasingCurve::InOutQuart);

  connect(ui->pushButton, &QPushButton::clicked, this, [this]() {
    if (is_expanded) {
      animation->setStartValue(ui->contentFrame->height());
      animation->setEndValue(0);
      ui->pushButton->setText(">" );
    } else {
      animation->setStartValue(0);
      animation->setEndValue(200);
      ui->pushButton->setText("V");
    }
    animation->start();
    is_expanded = !is_expanded;
  });
}

ImagePanelWidget::~ImagePanelWidget() {
  delete ui;
}

void ImagePanelWidget::setImage(const QPixmap& pixmap) {
  ui->lblImage->setPixmap(pixmap.scaled(200, 150, Qt::KeepAspectRatio));
}

