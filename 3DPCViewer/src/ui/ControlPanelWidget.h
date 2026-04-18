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
  void requestProcessBag(const QString& path);
  void requestUpdateFileSize(const int& size);
  void requestUpdatePointSize(const int& num);
  void topicListReady(const std::vector<std::string>& topics);
  void messageNumReady(int num);
  void imageFrameReady(const ImageFrame& frame);
  void progressUpdated(int percent);
  void pointSizeChanged(const int& size);
  void pointOpacityChanged(const int& opacity);
  void bgColorChanged(const QColor& color);
  void requestRunSlam(const QString& algorithm, bool is_rt_preview);

 private:
  std::unique_ptr<Ui::ControlPanelWidgetClass> ui;
};

