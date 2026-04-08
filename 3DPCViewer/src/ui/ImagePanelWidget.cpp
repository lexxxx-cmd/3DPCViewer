#include "ImagePanelWidget.h"
#include "ui_ImagePanelWidget.h"

ImagePanelWidget::ImagePanelWidget(QWidget *parent)
	: QWidget(parent)
	, ui(new Ui::ImagePanelWidget())
{
	ui->setupUi(this);
	ui->contentFrame->setMaximumHeight(0);

	// 初始化动画：目标是 contentFrame 的 maximumHeight 属性
	m_animation = new QPropertyAnimation(ui->contentFrame, "maximumHeight");
	m_animation->setDuration(300);
	m_animation->setEasingCurve(QEasingCurve::InOutQuart);

	connect(ui->pushButton, &QPushButton::clicked, this, [this]() {
		if (m_isExpanded) {
			m_animation->setStartValue(ui->contentFrame->height());
			m_animation->setEndValue(0);
			ui->pushButton->setText(">");
		}
		else {
			m_animation->setStartValue(0);
			m_animation->setEndValue(200); // 展开后的高度，可以根据图片自适应
			ui->pushButton->setText("V");
		}
		m_animation->start();
		m_isExpanded = !m_isExpanded;
	});
}

ImagePanelWidget::~ImagePanelWidget()
{
	delete ui;
}
void ImagePanelWidget::setImage(const QPixmap& pixmap) {
	ui->lblImage->setPixmap(pixmap.scaled(200, 150, Qt::KeepAspectRatio));
}
