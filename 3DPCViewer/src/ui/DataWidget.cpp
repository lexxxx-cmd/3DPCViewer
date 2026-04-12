#include "ui/DataWidget.h"
#include <QFileDialog>

DataWidget::DataWidget(QWidget *parent)
    : QWidget(parent)
{
    ui = std::make_unique<Ui::DataWidgetClass>();
    ui->setupUi(this);

    connect(ui->pB_import_bag, &QPushButton::clicked, this, [this]() {
        QString fileName = QFileDialog::getOpenFileName(this, "Open File", ".",
            "Open files(*.bag)");

        if (fileName.isEmpty()) return;
        ui->lbl_filename->setText(fileName);

        QFileInfo info(fileName);
        ui->lbl_filesize->setText(QString::number(info.size() / 1024 / 1024));
        emit requestProcBag(fileName);
    });
}

void DataWidget::setImportInProgress(bool inProgress) {
    ui->pB_import_bag->setEnabled(!inProgress);
}

DataWidget::~DataWidget() = default;
