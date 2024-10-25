#ifndef DECODER_H
#define DECODER_H

#include <QVector>
#include <condition_variable>

extern "C"{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

class Decoder // 将传输过来的视频文件解析为yuv
{
public:
    explicit Decoder();
    ~Decoder();

    bool decode(const QString& url);
    void exit();

    inline uint32_t duraiton() const {return m_duration;}
    inline int audioIndex() const {return m_audioIndex;}
    inline int videoIndex() const {return m_videoIndex;}
    inline bool isExit() const {return m_exit.load();}
    inline int videoPktSerial() const {return m_videoPktQueue.serial;}
    inline AVCodecParameters *auidoCodecPar() const {return m_pAvFormatCtx->streams[m_audioIndex]->codecpar;}
    inline AVCodecParameters *videoCodecPar() const {return m_pAvFormatCtx->streams[m_videoIndex]->codecpar;}
    inline AVFormatContext *formatContext() const {return m_pAvFormatCtx;}

    int getAFrame(AVFrame *frame);
    int getRemainingVFrameSize();
    void seekTo(int32_t target);

private:
    void initVal(); //复用播放器 重置变量
    void demux();
    void audioDecode();
    void videoDecode();
    void clearQueueCache();
    void pushAFrame(AVFrame *frame);
    void pushVFrame(AVFrame *frame);


public:
    struct FFrame{
      AVFrame frame;
      int serial;
      double duration; //一帧的持续时间
      double pts;
    };

private:
    struct FPacket{
        AVPacket pkt;
        int serial;
    };

    struct FPacketQueue{
        QVector<FPacket> pktVec;
        int readIndex;
        int pushIndex;
        int size;
        int serial;
        std::mutex mutex;
        std::condition_variable cond;
    };

    struct FFrameQueue{
        QVector<FFrame> frameVec;
        int readIndex;
        int pushIndex;
        int size;
        int shown; // 已经显示过的帧
        std::mutex mutex;
        std::condition_variable cond;
    };

    struct FPktDecoder{
      AVCodecContext *codecCtx = nullptr;
      int serial;
    };

    std::atomic_bool m_exit;

    AVFormatContext *m_pAvFormatCtx;
    char m_errBuf[100];
    uint32_t m_duration;
    int m_videoIndex;
    int m_audioIndex;

    FPktDecoder m_audioPktDecoder;
    FPktDecoder m_videoPktDecoder;

    AVRational m_videoFrameRate;

    FPacketQueue m_audioPktQueue;
    FPacketQueue m_videoPktQueue;
    FFrameQueue m_audioFrameQueue;
    FFrameQueue m_videoFrameQueue;

    const int m_maxPktQueueSize;
    const int m_maxFrameQueueSize;

    // 是否进行跳转
    bool m_isSeek;
    bool m_vidSeek;
    bool m_audSeek;

    //跳转的绝对时间
    int64_t m_seekTarget;

public:
    // 获取上一帧
    FFrame *getLastVFrame();
    // 获取当前帧
    FFrame *getVFrame();
    // 获取下一帧
    FFrame *getNextVFrame();
    // 读索引后移一位
    void setNextVFrame();

private:
    void packetQueueFlush(FPacketQueue *queue);
    void pushPacket(FPacketQueue *queue, AVPacket *pkt);
    int getPacket(FPacketQueue *queue, AVPacket* destPkt, FPktDecoder *decoder);

};

#endif // DECODER_H
