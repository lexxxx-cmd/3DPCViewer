#include "interactionwidget.h"
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

	connect(ui->HSlider_size, &QSlider::valueChanged,
		this, &InteractionWidget::onSizeSliderChanged);

	connect(ui->HSlider_opacity, &QSlider::valueChanged,
		this, &InteractionWidget::onOpacitySliderChanged);

	onSizeSliderChanged(ui->HSlider_size->value());
	onOpacitySliderChanged(ui->HSlider_opacity->value());

	connect(ui->CBox_show_normals, &QCheckBox::clicked,
		this, &InteractionWidget::onNormalShow);
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

void InteractionWidget::onNormalShow(const bool& show)
{
	emit requestShowNormals(show);
}

