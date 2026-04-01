#include "statuswidget.h"

StatusWidget::StatusWidget(QWidget *parent)
	: QWidget(parent)
{
	ui = std::make_unique< Ui::StatusWidgetClass>();
	ui->setupUi(this);
}

StatusWidget::~StatusWidget() = default;


