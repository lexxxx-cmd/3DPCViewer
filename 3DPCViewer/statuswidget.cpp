#include "statuswidget.h"

StatusWidget::StatusWidget(QWidget *parent)
	: QWidget(parent)
{
	ui = std::make_unique< Ui::StatusWidgetClass>();
	ui->setupUi(this);
}

StatusWidget::~StatusWidget() = default;


void StatusWidget::updatePointSize(const int& size) {
	ui->lbl_pointsize->setText(QString::number(size));
}

void StatusWidget::updateFPS(const int& fps) {
	ui->lbl_fps->setText(QString::number(fps));
}

