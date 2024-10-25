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
    virtual void paintEvent(QPaintEvent *event) override;
    virtual void resizeEvent(QResizeEvent *event) override;
    virtual void keyReleaseEvent(QKeyEvent *event) override;

private:
    void initUi();

private slots:
    void addFile();
    void playSlot();
    void durationChangedSlot(uint32_t duration);
    void terminateSlot();
    void avPtsChangedSlot(unsigned int pts);
    void setVolume(int volume);
    void pauseSlot();
    void ptsSliderPressedSlot();
    void ptsSliderMovedSlot(int position);
    void ptsSliderReleaseSlot();
    void seekForwardSlot();
    void seekBackSlot();
private:
    Ui::Widget *ui;
    AVPlayer *m_player;

    QString m_formatFilter;
    uint32_t m_duration;

    bool m_ptsSliderPressed;
    int m_seekTarget;
};

#endif // WIDGET_H
