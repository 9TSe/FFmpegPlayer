#ifndef AVPLAYER_H
#define AVPLAYER_H
#include <QObject>
#include "Decoder.h"

extern "C"{
#include <SDL.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

class AVPlayer : public QObject
{
    Q_OBJECT
public:
    explicit AVPlayer(QObject *parent = nullptr);
    ~AVPlayer();
    bool play(const QString& url);
    void initPlayer();


private:
    bool initSDL();
    static bool compareChannelLayouts(const AVChannelLayout *layout1, const AVChannelLayout *layout2);
    static void fillAudioStreamCallback(void* userData, uint8_t *stream, int len);

signals:
    void durationChanged(uint32_t duration);
    void avTerminate();
private:
    Decoder *m_decoder;
    uint32_t m_duration;
    //视频的停止/暂停?
    bool m_pause;
    //同步时钟初始化标志, 音视频异步线程
    //谁先读到标志位, 谁先初始化时钟
    bool m_clockInitFlag;

    //SDL停止标志?
    bool m_exit;

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


    uint8_t *m_buffer;
    uint32_t m_imageWidth;
    uint32_t m_imageHeight;
};

#endif // AVPLAYER_H
