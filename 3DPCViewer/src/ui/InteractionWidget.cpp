#include "ui/InteractionWidget.h"
#include <QSlider>
#include <QCheckBox>

InteractionWidget::InteractionWidget(QWidget *parent)
	: QWidget(parent)
{
	ui = std::make_unique< Ui::InteractionWidgetClass>();
	ui->setupUi(this);
	//
	ui->HSlider_size->setMaximum(10);
	ui->HSlider_size->setMinimum(1);
	ui->HSlider_size->setValue(1);
	ui->HSlider_size->setSingleStep(1);

	ui->HSlider_opacity->setMaximum(100);
	ui->HSlider_opacity->setMinimum(10);
	ui->HSlider_opacity->setValue(100);
	ui->HSlider_opacity->setSingleStep(10);
	
	ui->HSlider_progress->setMaximum(max_messageNum);
	ui->HSlider_progress->setMinimum(0);
	ui->HSlider_progress->setValue(0);
	ui->HSlider_progress->setSingleStep(1);

	ui->pB_backward->setEnabled(false);
	ui-> pB_play_pause->setEnabled(false);
	ui->pB_forward->setEnabled(false);

	connect(ui->HSlider_size, &QSlider::valueChanged,
		this, &InteractionWidget::onSizeSliderChanged);

	connect(ui->HSlider_opacity, &QSlider::valueChanged,
		this, &InteractionWidget::onOpacitySliderChanged);

	m_timer = new QTimer(this);
	m_timer->setInterval(100);

	connect(m_timer, &QTimer::timeout, this, [this]() {
		if (cur_messageNum < max_messageNum) {
			ui->HSlider_progress->setValue(cur_messageNum + 1);
		}
		else {
			// 播放到最后自动停止
			m_timer->stop();
			isPlay = false;
			ui->pB_play_pause->setText("Play");
			ui->pB_backward->setEnabled(true);
			// pB_forward 保持禁用，因为已经到底了
		}
	});

	connect(ui->HSlider_progress, &QSlider::valueChanged,
		this, &InteractionWidget::onProgressNumChanged);

	connect(ui->pB_backward, &QPushButton::clicked, this, [this]() {
		if (cur_messageNum > 0) {
			ui->HSlider_progress->setValue(cur_messageNum - 1);
		}
		else {
			ui->pB_backward->setEnabled(false);
		}
	});

	connect(ui->pB_forward, &QPushButton::clicked, this, [this]() {
		if (cur_messageNum < max_messageNum) {
			ui->HSlider_progress->setValue(cur_messageNum + 1);
		}
		else {
			ui->pB_forward->setEnabled(false);
		}
	});

	connect(ui->pB_play_pause, &QPushButton::clicked, this, [this]() {
		isPlay = !isPlay;
		if (isPlay) {
			ui->pB_play_pause->setText("Pause");
			ui->pB_backward->setEnabled(false);
			ui->pB_forward->setEnabled(false);
			m_timer->start();
		}
		else {
			ui->pB_play_pause->setText("Play");
			ui->pB_backward->setEnabled(true);
			ui->pB_forward->setEnabled(true);
			m_timer->stop();
		}
	});
	onSizeSliderChanged(ui->HSlider_size->value());
	onOpacitySliderChanged(ui->HSlider_opacity->value());

}

InteractionWidget::~InteractionWidget() = default;

void InteractionWidget::onSizeSliderChanged(int value)
{
	ui->lbl_PSize->setText(QString("%1").arg(value));
	emit pointSizeChanged(value);
}

// 透明度滑块
void InteractionWidget::onOpacitySliderChanged(int value)
{
	// 透明度转百分比显示
	ui->lbl_Popacity->setText(QString("%1%").arg(value));
	emit pointOpacityChanged(value);
}

void InteractionWidget::onMaxmessageNumSet(int value) {
	max_messageNum = value;
	if (max_messageNum > 0) {
		ui->pB_play_pause->setEnabled(true);
		ui->pB_forward->setEnabled(true);
	}
	ui->HSlider_progress->setMaximum(max_messageNum);
	ui->lbl_ProgressValue->setText(QString("%1/%2").arg(ui->HSlider_progress->value()).arg(max_messageNum));

}

void InteractionWidget::onProgressNumChanged(int value) {
	cur_messageNum = value;
	if (cur_messageNum > 0) {
		ui->pB_backward->setEnabled(true);
	}
	else {
		ui->pB_backward->setEnabled(false);
	}
	ui->lbl_ProgressValue->setText(QString("%1/%2").arg(cur_messageNum).arg(max_messageNum));
	emit progressUpdated(cur_messageNum);
}