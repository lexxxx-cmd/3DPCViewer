#include "datawidget.h"
#include <QFileDialog>

DataWidget::DataWidget(QWidget *parent)
	: QWidget(parent)
{
	ui = std::make_unique<Ui::DataWidgetClass>();
	ui->setupUi(this);

	connect(ui->pB_import_data, &QPushButton::clicked, this, [this]() {
		QString fileName = QFileDialog::getOpenFileName(this, "Open PointCloud", ".",
			"Open files(*.pcd *.ply *.obj)");
		if (fileName.isEmpty()) return;
		ui->lbl_filename->setText(fileName);
		emit requestLoadFile(fileName);
	});
}

DataWidget::~DataWidget() = default;


void DataWidget::updateFileSize(const int& size) {
	ui->lbl_filesize->setText(QString::number(size));
}
