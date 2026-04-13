#pragma once

#include <QWidget>
#include <QPropertyAnimation>

QT_BEGIN_NAMESPACE
namespace Ui { class ImagePanelWidget; };
QT_END_NAMESPACE

class ImagePanelWidget : public QWidget {
  Q_OBJECT

 public:
  ImagePanelWidget(QWidget* parent = nullptr);
  ~ImagePanelWidget();
  void setImage(const QPixmap& pixmap);

 private:
  Ui::ImagePanelWidget* ui;
  bool is_expanded = false;
  QPropertyAnimation* animation;
};
