#include "ui/DataWidget.h"
#include <QFileDialog>

DataWidget::DataWidget(QWidget* parent) : QWidget(parent) {
  ui = std::make_unique<Ui::DataWidgetClass>();
  ui->setupUi(this);

  connect(ui->pB_import_bag, &QPushButton::clicked, this, [this]() {
    QString file_name = QFileDialog::getOpenFileName(this, "Open File", ".",
        "Open files(*.bag)");

    if (file_name.isEmpty()) return;
    ui->lbl_filename->setText(file_name);

    QFileInfo info(file_name);
    ui->lbl_filesize->setText(QString::number(info.size() / 1024 / 1024));
    emit requestProcessBag(file_name);
  });
}

DataWidget::~DataWidget() = default;


