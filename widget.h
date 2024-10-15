#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
class Widget;
}
QT_END_NAMESPACE

class AVPlayer;

class Widget : public QWidget
{
    Q_OBJECT

public:
    explicit Widget(QWidget *parent = nullptr);
    ~Widget();

protected:
//    virtual void paintEvent(QPaintEvent *event) override;
//    virtual void resizeEvent(QResizeEvent *event) override;
//    virtual void keyReleaseEvent(QKeyEvent* event) override;

private:
    Ui::Widget *ui;
    AVPlayer *m_player;
};

#endif // WIDGET_H