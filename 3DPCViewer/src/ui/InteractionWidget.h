#pragma once

#include <QWidget>
#include <memory>
#include <QTimer>
#include <QColor>
#include "ui_InteractionWidget.h"
#include "io/BagDataTypes.h"

QT_BEGIN_NAMESPACE
namespace Ui { class InteractionWidgetClass; };
QT_END_NAMESPACE

class InteractionWidget : public QWidget {
  Q_OBJECT

 public:
  InteractionWidget(QWidget* parent = nullptr);
  ~InteractionWidget();

 public slots:
  void onSizeSliderChanged(int value);
  void onOpacitySliderChanged(int value);
  void onMaxMessageNumSet(int value);
  void onProgressNumChanged(int value);
  void onImageFrameReady(const ImageFrame& frame);

 signals:
  void pointSizeChanged(const int& value);
  void pointOpacityChanged(const int& value);
  void progressUpdated(const int value);
  void bgColorChanged(const QColor& color);

 private:
  std::unique_ptr<Ui::InteractionWidgetClass> ui;
  QTimer* timer;
  int max_message_num = 0;
  int cur_message_num = 0;
  bool is_play = false;
};

