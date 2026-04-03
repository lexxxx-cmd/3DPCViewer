#include "ImagePanel.h"
#include "ui_ImagePanel.h"

ImagePanel::ImagePanel(QWidget* parent) : QWidget(parent), ui(new Ui::ImagePanel) {
    ui->setupUi(this);

    // 初始状态：折叠
    ui->contentFrame->setMaximumHeight(0);

    // 初始化动画：目标是 contentFrame 的 maximumHeight 属性
    m_animation = new QPropertyAnimation(ui->contentFrame, "maximumHeight");
    m_animation->setDuration(300);
    m_animation->setEasingCurve(QEasingCurve::InOutQuart);
}

void ImagePanel::on_btnToggle_clicked() {
    if (m_isExpanded) {
        m_animation->setStartValue(ui->contentFrame->height());
        m_animation->setEndValue(0);
        ui->btnToggle->setText("显示图片");
    }
    else {
        m_animation->setStartValue(0);
        m_animation->setEndValue(200); // 展开后的高度，可以根据图片自适应
        ui->btnToggle->setText("隐藏图片");
    }
    m_animation->start();
    m_isExpanded = !m_isExpanded;
}

void ImagePanel::setImage(const QPixmap& pixmap) {
    ui->lblImage->setPixmap(pixmap.scaled(200, 150, Qt::KeepAspectRatio));
}
