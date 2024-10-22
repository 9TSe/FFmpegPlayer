#include "widget.h"
#include "ui_widget.h"
#include "AVPlayer.h"
#include <QFileDialog>

Widget::Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget),
    m_formatFilter("video file(*.mp4 *.flv *.ts)")
{
    ui->setupUi(this);
    QString title = QString("%1 V%2 (64-bit Windows)").arg(QCoreApplication::applicationName()).arg(QCoreApplication::applicationVersion());
    setWindowTitle(title);

    m_player = new AVPlayer(this);

    initUi(); //初始ui中控件等属性


    ui->opengl_widget->show(); //test openGL
    connect(ui->btn_addFile, &QPushButton::clicked, this, &Widget::addFile);
    connect(ui->btn_play, &QPushButton::clicked, this, &Widget::playSlot);
    connect(m_player, &AVPlayer::durationChanged, this, &Widget::durationChangedSlot);
    // Queueconnection 能够确保线程安全 反例(另一个线程 调用主线程的槽函数
    // 音视频结束
    connect(m_player, &AVPlayer::avTerminate, this, &Widget::terminateSlot, Qt::QueuedConnection);
}

Widget::~Widget()
{
    delete ui;
}


void Widget::playSlot()
{
    const QString url = ui->lineEdit_input->text();
    if(url.count()){
        m_player->play(url);
    }
}


void Widget::initUi()
{
    ui->label_pts->setAlignment(Qt::AlignCenter);
    ui->label_volume->setAlignment(Qt::AlignCenter);
    ui->label_duration->setAlignment(Qt::AlignCenter);
    ui->label_seperator->setAlignment(Qt::AlignCenter);

    //ui->label_volume->setText("30");
    ui->lineEdit_input->setText("D:/ffmpeg/learn/miku.mp4");

    ui->btn_back->setEnabled(false);
    ui->btn_pauseon->setEnabled(false);
    ui->btn_forward->setEnabled(false);

    //ui->slider_AVPts->setEnabled(false);
    ui->slider_volume->setMaximum(100);
    ui->slider_volume->setValue(50);
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

}
