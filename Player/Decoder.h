#ifndef DECODER_H
#define DECODER_H

#include <QVector>


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


private:


private:
    AVFormatContext *m_pAvFormatCtx;
    char m_errBuf[100];
    uint32_t m_duration;
    int m_videoIndex;
    int m_audioIndex;
};

#endif // DECODER_H
