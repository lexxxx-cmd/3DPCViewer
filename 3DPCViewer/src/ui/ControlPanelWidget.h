#pragma once

#include <QWidget>
#include <memory>
#include <QColor>
#include "ui_ControlPanelWidget.h"
#include "ui/DataWidget.h"
#include "ui/StatusWidget.h"
#include "ui/InteractionWidget.h"

QT_BEGIN_NAMESPACE
namespace Ui { class ControlPanelWidgetClass; };
QT_END_NAMESPACE

class ControlPanelWidget : public QWidget {
  Q_OBJECT

 public:
  ControlPanelWidget(QWidget* parent = nullptr);
  ~ControlPanelWidget() = default;

  DataWidget* getDataWidget() const { return ui->DataW; }
  StatusWidget* getStatusWidget() const { return ui->StatusW; }
  InteractionWidget* getInteractionWidget() const { return ui->InterWidget; }

 public slots:
  void onFileSizeUpdated(const int& size);
  void onPointSizeUpdated(const int& num);

 signals:
  void requestLoadFile(const QString& path);
  void requestProcessBag(const QString& path);
  void requestUpdateFileSize(const int& size);
  void requestUpdatePointSize(const int& num);
  void requestUpdateFps(const int& fps);
  void requestShowNormals(const bool& show);

  void pointSizeChanged(const int& value);
  void pointOpacityChanged(const int& value);
  void bgColorChanged(const QColor& color);

  void topicListReady(const std::vector<std::string>& topics);
  void messageNumReady(int num);
  void progressUpdated(const int value);
  void imageFrameReady(const ImageFrame& frame);

 private:
  std::unique_ptr<Ui::ControlPanelWidgetClass> ui;
};

