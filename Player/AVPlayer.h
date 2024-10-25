#ifndef AVPLAYER_H
#define AVPLAYER_H
#include <QObject>
#include "Decoder.h"

extern "C"{
#include <SDL.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/time.h>
#include <libavutil/imgutils.h>
#include <libavdevice/avdevice.h>
}

//同步阈值下限
#define AV_SYNC_THRESHOLD_MIN 0.04
//同步阈值上限
#define AV_SYNC_THRESHOLD_MAX 0.1
//单帧视频时长阈值上限，用于适配低帧时同步，
//帧率过低视频帧超前不适合翻倍延迟，应特殊
//处理，这里设置上限一秒10帧
#define AV_SYNC_FRAMEDUP_THRESHOLD 0.1
//同步操作摆烂阈值上限，此时同步已无意义
#define AV_NOSYNC_THRESHOLD 10.0

#define AV_SYNC_REJUDGESHOLD 0.01


class YUV422Frame;

class AVClock
{
public:
    AVClock()
        :m_pts(0.0),
          m_drift(0.0)
    {}
    inline void resetClock()
    {
        m_pts = 0.0;
        m_drift = 0.0;
    }
    inline void setClock(double pts)
    {
        setClockAt(pts);
    }
    inline double getClock()
    {
        return m_drift + av_gettime_relative() / 1000000.0;
    }
private:
    inline void setClockAt(double pts)
    {
        m_drift = pts - av_gettime_relative() / 1000000.0;
        m_pts = pts;
    }

    double m_pts;
    double m_drift;
};


class AVPlayer : public QObject
{
    Q_OBJECT

public:
    explicit AVPlayer(QObject *parent = nullptr);
    ~AVPlayer();
    bool play(const QString& url);
    void handlePauseClick(bool isPause);
    void initPlayer();
    static bool compareChannelLayouts(const AVChannelLayout *layout1, const AVChannelLayout *layout2);

    inline void setVolume(int volume){
        m_volume = (volume * SDL_MIX_MAXVOLUME / 100) % (SDL_MIX_MAXVOLUME + 1);
    }
    inline int getVolume() const{return m_volume;}

    enum PlayState{
        AV_STOPPED,
        AV_PLAYING,
        AV_PAUSED,
        AV_NONE
    };
    AVPlayer::PlayState getState();

    void seekTo(int32_t time_s);
    void seekBy(int32_t time_s);

private:
    bool initSDL();
    void initVideo();
    void initAVClock();
    void videoCallback();
    double frameDuration(Decoder::FFrame *lastFrame, Decoder::FFrame *currentFrame);
    double computeTargetDelay(double delay);
    void disPlayImage(AVFrame *frame);

    static void fillAudioStreamCallback(void* userData, uint8_t *stream, int len);

signals:
    void durationChanged(uint32_t duration);
    void avTerminate();
    void avPtsChanged(unsigned int pts);
    void frameChanged(QSharedPointer<YUV422Frame> frame);
private:
    Decoder *m_decoder;
    uint32_t m_duration;
    //音视频的暂停
    bool m_pause;
    // 记录上一次暂停的时间
    double m_pauseTime;
    //同步时钟初始化标志, 音视频异步线程
    //谁先读到标志位, 谁先初始化时钟
    bool m_clockInitFlag;

    // 音视频停止
    bool m_exit;

    // SDL buf
    uint8_t *m_audioBuf;
    uint32_t m_audioBufSize;
    uint32_t m_audioBufIndex;
    uint32_t m_lastAudioPts;

    AVCodecParameters *m_audioCodecPar;
    AVFormatContext *m_fmtCtx;

    enum AVSampleFormat m_targetSampleFmt; //位深
    int m_targetChannels;
    int m_targetFreq;
    //int m_targetChannelLayout; 已弃用
    AVChannelLayout m_targetChannelLayout;
    int m_targetNbSamples;

    int m_audioIndex;
    int m_videoIndex;

    AVFrame *m_audioFrame;
    SwsContext *m_swsCtx;
    SwrContext *m_swrCtx;

    int m_volume;

    AVClock m_audioClock;
    AVClock m_videoClock;

    double m_frameTimer;
    AVCodecParameters *m_videoCodecPar;
    int m_imageWidth;
    int m_imageHeight;
    float m_aspectRatio = 1; // 宽高比

    enum AVPixelFormat m_dstPixFmt;
    int m_swsFlag;
    double m_delay; // delaytime

    // 指向存储每个平面（如 YUV 图像的 Y、U、V 分量）的数据指针的数组
    uint8_t* m_pixels[4];
    // 指向每个平面的行大小（以字节为单位）的数组, 也可以称为步幅
    int m_pitch[4];
    // // 保存的是单个图片的数据
    uint8_t *m_videoBuf;

};

#endif // AVPLAYER_H
