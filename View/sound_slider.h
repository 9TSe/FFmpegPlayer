#ifndef SOUNDSLIDER_H
#define SOUNDSLIDER_H

#include <QSlider>

class SoundSlider : public QSlider
{
    Q_OBJECT
public:
    SoundSlider(QWidget *parent = nullptr);
    ~SoundSlider() = default;
protected:
    virtual bool event(QEvent *e) override;
    virtual void mousePressEvent(QMouseEvent *ev) override;
    virtual void mouseMoveEvent(QMouseEvent *ev) override;

signals:
    void volumeChanged(int volume);

};

#endif // SOUNDSLIDER_H
