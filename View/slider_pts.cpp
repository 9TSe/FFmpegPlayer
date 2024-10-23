#include "slider_pts.h"
#include <QMouseEvent>

AVPtsSlider::AVPtsSlider(QWidget *parent)
    :QSlider(parent),
      m_isEnter(false),
      m_ptsPercent(0.00),
      m_cursorXPer(0.00)
{
    setMouseTracking(true);
}

void AVPtsSlider::setPtsPercent(double ptsPercent)
{
    if(ptsPercent < 0.0 || ptsPercent > 1.0) return;
    m_ptsPercent = ptsPercent;
    setValue((int)ptsPercent * this->width());
}

double AVPtsSlider::getPtsPercent()
{
    return (double)this->value() / this->width();
}

bool AVPtsSlider::event(QEvent *e)
{
    if(e->type() == QEvent::Resize){
        setMaximum(this->width() - 1);
        setPtsPercent(m_ptsPercent);
        m_ptsPercent = getPtsPercent();
    }
    return QSlider::event(e);
}

void AVPtsSlider::mouseMoveEvent(QMouseEvent *ev)
{
    int posX = ev->pos().x() < 0 ? 0 : ev->pos().x() > this->width() - 1 ? this->width() - 1 : ev->pos().x();
    m_cursorXPer = (double)posX / this->width();
    if(m_isEnter){
        setValue(posX);
    }
    emit sliderMoved(posX);
}

void AVPtsSlider::mousePressEvent(QMouseEvent *ev)
{
    setValue(ev->pos().x());
    emit sliderPressed();
    m_isEnter = true;
}

void AVPtsSlider::mouseReleaseEvent(QMouseEvent *ev)
{
    Q_UNUSED(ev);
    m_ptsPercent = getPtsPercent();
    emit sliderReleased();
    m_isEnter = false;
}

void AVPtsSlider::paintEvent(QPaintEvent *ev)
{
    QSlider::paintEvent(ev);
}
