#include "widget.h"
#include "ui_widget.h"
#include "AVPlayer.h"
#include "YUV422Frame.h"
#include "MsgBox.h"
#include <QFileDialog>
#include <QPainter>

// 声名后才能使用关于这个类型的槽函数
Q_DECLARE_METATYPE(QSharedPointer<YUV422Frame>)

Widget::Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget),
    m_formatFilter("video file(*.mp4 *.flv *.ts)"),
    m_duration(0),
    m_ptsSliderPressed(false),
    m_seekTarget(0)
{
    ui->setupUi(this);
    QString title = QString("%1 V%2 (64-bit Windows)").arg(QCoreApplication::applicationName()).arg(QCoreApplication::applicationVersion());
    setWindowTitle(title);

    m_player = new AVPlayer(this);

    initUi(); //初始ui中控件等属性

    // 展现视频
    connect(m_player, &AVPlayer::frameChanged, ui->opengl_widget, &OpenGLWidget::showYUV, Qt::QueuedConnection);

    // 添加文件
    connect(ui->btn_addFile, &QPushButton::clicked, this, &Widget::addFile);
    // 播放
    connect(ui->btn_play, &QPushButton::clicked, this, &Widget::playSlot);
    // 新视频加入更新视频总时长
    connect(m_player, &AVPlayer::durationChanged, this, &Widget::durationChangedSlot);
    // Queueconnection 能够确保线程安全 反例(另一个线程 调用主线程的槽函数
    // 音视频结束
    connect(m_player, &AVPlayer::avTerminate, this, &Widget::terminateSlot, Qt::QueuedConnection);
    // 视频进度条
    connect(m_player, &AVPlayer::avPtsChanged, this, &Widget::avPtsChangedSlot);
    // 音频进度条
    connect(ui->slider_volume, &SoundSlider::volumeChanged, this, &Widget::setVolume);
    // 暂停
    connect(ui->btn_pauseon, &QPushButton::clicked, this, &Widget::pauseSlot);
    connect(ui->opengl_widget, &OpenGLWidget::mouseClicked, this, &Widget::pauseSlot);
    // 双击屏幕
    connect(ui->opengl_widget, &OpenGLWidget::mouseDoubleClicked, [&](){
        if(isMaximized()) showNormal();
        else showMaximized();
    });
    // 视频进度条槽
    connect(ui->slider_AVPts, &AVPtsSlider::sliderPressed, this, &Widget::ptsSliderPressedSlot);
    connect(ui->slider_AVPts, &AVPtsSlider::sliderMoved, this, &Widget::ptsSliderMovedSlot);
    connect(ui->slider_AVPts, &AVPtsSlider::sliderReleased, this, &Widget::ptsSliderReleaseSlot);

    // 快进后退
    connect(ui->btn_forward, &QPushButton::clicked, this, &Widget::seekForwardSlot);
    connect(ui->btn_back, &QPushButton::clicked, this, &Widget::seekBackSlot);
}

Widget::~Widget()
{
    delete ui;
}


void Widget::playSlot()
{
    const QString url = ui->lineEdit_input->text();
    if(url.count()){
        if(m_player->play(url)){
            ui->btn_play->setEnabled(false);
            ui->btn_forward->setEnabled(true);
            ui->btn_back->setEnabled(true);
            ui->btn_pauseon->setEnabled(true);
            ui->slider_AVPts->setEnabled(true);
        }
    }
    else{
        MsgBox::error(nullptr, "路径不能为空");
    }
}


void Widget::initUi()
{
    setWindowIcon(QIcon(":/res/images/zoe2.PNG"));
    ui->label_pts->setAlignment(Qt::AlignCenter);
    ui->label_volume->setAlignment(Qt::AlignCenter);
    ui->label_duration->setAlignment(Qt::AlignCenter);
    ui->label_seperator->setAlignment(Qt::AlignCenter);

    ui->lineEdit_input->setText("D:/ffmpeg/learn/test.mp4");

    ui->btn_back->setEnabled(false);
    ui->btn_pauseon->setEnabled(false);
    ui->btn_forward->setEnabled(false);

    //ui->slider_AVPts->setEnabled(false);
    ui->slider_volume->setMaximum(100);
    ui->slider_volume->setValue(m_player->getVolume() * 2);
    ui->label_volume->setText(QString("%1%").arg(m_player->getVolume()));
}

void Widget::addFile()
{
    QString url = QFileDialog::getOpenFileName(this, "chose file", QDir::currentPath(), m_formatFilter);
    ui->lineEdit_input->setText(url);
}

void Widget::durationChangedSlot(uint32_t duration)
{
    ui->label_duration->setText(QString("%1:%2").arg(duration / 60, 2, 10, QLatin1Char('0'))
                                                .arg(duration % 60, 2, 10, QLatin1Char('0')));
    m_duration = duration;
}

void Widget::terminateSlot()
{
    ui->label_pts->setText(QString("00:00"));
    ui->label_duration->setText(QString("00:00"));
    ui->slider_AVPts->setEnabled(false);
    ui->slider_AVPts->setValue(0);
    ui->btn_forward->setEnabled(false);
    ui->btn_back->setEnabled(false);
    ui->btn_pauseon->setEnabled(false);
    ui->btn_pauseon->setText(QString("暂停"));
    ui->btn_play->setEnabled(true);
    m_player->initPlayer();
    MsgBox::success(this, "视频已播放完毕");
}

void Widget::avPtsChangedSlot(unsigned int pts)
{
    if(m_ptsSliderPressed) return;
    ui->slider_AVPts->setPtsPercent((double)pts / m_duration);
    ui->label_pts->setText(QString("%1:%2").arg(pts / 60, 2, 10, QLatin1Char('0'))
                                        .arg(pts % 60, 2, 10, QLatin1Char('0')));
}

void Widget::setVolume(int volume)
{
    m_player->setVolume(volume);
    ui->label_volume->setText(QString("%1%").arg(volume));
}

void Widget::pauseSlot()
{
    switch(m_player->getState()){
    case AVPlayer::AV_PLAYING:
        m_player->handlePauseClick(true);
        ui->btn_pauseon->setText("继续");
        break;
    case AVPlayer::AV_PAUSED:
        m_player->handlePauseClick(false);
        ui->btn_pauseon->setText("暂停");
        break;
    default:
        break;
    }
}

void Widget::ptsSliderPressedSlot()
{
    m_ptsSliderPressed = true;
    m_seekTarget = (int)(ui->slider_AVPts->getPtsPercent() * m_duration);
}

void Widget::ptsSliderMovedSlot(int position)
{
    Q_UNUSED(position);
    m_seekTarget = (int)(ui->slider_AVPts->getCursorXPer() * m_duration);
    const QString& ptsStr = QString("%1:%2").arg(m_seekTarget / 60, 2, 10, QLatin1Char('0'))
                                            .arg(m_seekTarget % 60, 2, 10, QLatin1Char('0'));
    if(m_ptsSliderPressed){
        ui->label_pts->setText(ptsStr);
    }else{
        ui->slider_AVPts->setToolTip(ptsStr); // 指着进度条
    }
}

void Widget::ptsSliderReleaseSlot()
{
    m_player->seekTo(m_seekTarget);
    m_ptsSliderPressed = false;
}

void Widget::seekForwardSlot()
{
    m_player->seekBy(5);
    if(m_player->getState() == AVPlayer::AV_PAUSED){
        m_player->handlePauseClick(false); // 重新开始
    }
}

void Widget::seekBackSlot()
{
    m_player->seekBy(-5);
    if(m_player->getState() == AVPlayer::AV_PAUSED){
        m_player->handlePauseClick(false); // 重新开始
    }
}

void Widget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
//    QPainter painter(this);
//    painter.setBrush(QBrush(QColor(46, 46, 54)));
//    painter.drawRect(rect());
}
void Widget::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event);
}
void Widget::keyReleaseEvent(QKeyEvent *event)
{
    Q_UNUSED(event);
}
