#include "Decoder.h"
#include "ThreadPool.h"
#include <QsLog.h>

Decoder::Decoder()
    :m_pAvFormatCtx(nullptr)
{
    ThreadPool::instance();
}

Decoder::~Decoder()
{

}

bool Decoder::decode(const QString &url) //从视频内部获取信息填充成员变量
{
    m_pAvFormatCtx = avformat_alloc_context();

    AVDictionary *fmtOpt = nullptr;
    av_dict_set(&fmtOpt, "probesize", "32", 0);

    int errorNum = avformat_open_input(&m_pAvFormatCtx, url.toUtf8().constData(), nullptr, nullptr);
    if(errorNum < 0){
        av_strerror(errorNum, m_errBuf, sizeof(m_errBuf));
        QLOG_ERROR() << "avformat_open_input fail: " << m_errBuf;
        av_dict_free(&fmtOpt);
        return false;
    }

    errorNum = avformat_find_stream_info(m_pAvFormatCtx, nullptr);
    if(errorNum < 0){
        av_strerror(errorNum, m_errBuf, sizeof(m_errBuf));
        QLOG_ERROR() << "avformat_find_stream_info fail: " << m_errBuf;
        av_dict_free(&fmtOpt);
        return false;
    }

    AVRational ratio = {1, AV_TIME_BASE}; // 1 / 1000000
    m_duration = (uint32_t)m_pAvFormatCtx->duration * av_q2d(ratio);
    av_dict_free(&fmtOpt);



    return 1;
}
