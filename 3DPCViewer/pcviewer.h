#pragma once

#include <QtWidgets/QMainWindow>
#include <memory>
#include "ui_pcviewer.h"
#include "controlpanelwidget.h"
#include "visualareawidget.h"

QT_BEGIN_NAMESPACE
namespace Ui { class PCViewerClass; };
QT_END_NAMESPACE

class PCViewer : public QMainWindow
{
    Q_OBJECT

public:
    PCViewer(QWidget *parent = nullptr);
    ~PCViewer();

private:
    std::unique_ptr<Ui::PCViewerClass> ui;

};

