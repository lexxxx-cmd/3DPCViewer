#pragma once

#include <QWidget>
#include <QPropertyAnimation>

namespace Ui { class ImagePanel; }

class ImagePanel : public QWidget {
    Q_OBJECT
public:
    explicit ImagePanel(QWidget* parent = nullptr);
    void setImage(const QPixmap& pixmap);

private slots:
    void on_btnToggle_clicked();

private:
    Ui::ImagePanel* ui;
    bool m_isExpanded = false;
    QPropertyAnimation* m_animation;
};