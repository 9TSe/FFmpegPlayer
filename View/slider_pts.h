#ifndef AVSLIDER_H
#define AVSLIDER_H

#include <QSlider>

class AVPtsSlider : public QSlider
{
    Q_OBJECT
public:
    explicit AVPtsSlider(QWidget *parent = nullptr);
    ~AVPtsSlider() = default;
    void setPtsPercent(double ptsPercent);
    double getPtsPercent();
    inline double getCursorXPer() const {return m_cursorXPer;}

protected:
    virtual bool event(QEvent *e) override;
    virtual void paintEvent(QPaintEvent *ev) override;
    virtual void mouseMoveEvent(QMouseEvent *ev) override;
    virtual void mousePressEvent(QMouseEvent *ev) override;
    virtual void mouseReleaseEvent(QMouseEvent *ev) override;

private:
    bool m_isEnter; //检测鼠标按下
    double m_ptsPercent;
    double m_cursorXPer; //鼠标横比
};


#endif // AVSLIDER_H
