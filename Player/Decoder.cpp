#include "Decoder.h"
#include "ThreadPool.h"
#include <QsLog.h>

Decoder::Decoder()
    : m_exit(false),
      m_pAvFormatCtx(nullptr),
      m_duration(0),
      m_videoIndex(-1),
      m_audioIndex(-1),
      m_maxPktQueueSize(32),
      m_maxFrameQueueSize(16)
{
    ThreadPool::instance();
    m_audioPktQueue.pktVec.resize(m_maxPktQueueSize);
    m_videoPktQueue.pktVec.resize(m_maxPktQueueSize);
    m_audioFrameQueue.frameVec.resize(m_maxFrameQueueSize);
    m_videoFrameQueue.frameVec.resize(m_maxFrameQueueSize);
    m_audioPktDecoder.codecCtx = nullptr;
    m_videoPktDecoder.codecCtx = nullptr;
    initVal();
}

Decoder::~Decoder()
{
    exit();
}

void Decoder::initVal()
{
    m_exit.store(false);
    m_audioPktQueue.size = 0;
    m_audioPktQueue.serial = 0;
    m_audioPktQueue.pushIndex = 0;
    m_audioPktQueue.readIndex = 0;
    m_videoPktQueue.size = 0;
    m_videoPktQueue.serial = 0;
    m_videoPktQueue.pushIndex = 0;
    m_videoPktQueue.readIndex = 0;

    m_audioFrameQueue.size = 0;
    m_audioFrameQueue.shown = 0;
    m_audioFrameQueue.pushIndex = 0;
    m_audioFrameQueue.readIndex = 0;
    m_videoFrameQueue.size = 0;
    m_videoFrameQueue.shown = 0;
    m_videoFrameQueue.pushIndex = 0;
    m_videoFrameQueue.readIndex = 0;

    m_audioPktDecoder.serial = 0;
    m_videoPktDecoder.serial = 0;

    m_isSeek = false;
    m_audSeek = false;
    m_vidSeek = false;
}

void Decoder::exit()
{
    if(m_exit.load()) return;
    m_exit.store(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    clearQueueCache();
    if(m_pAvFormatCtx != nullptr){
        avformat_close_input(&m_pAvFormatCtx);
        m_pAvFormatCtx = nullptr;
    }
    if(m_audioPktDecoder.codecCtx != nullptr){
        avcodec_free_context(&m_audioPktDecoder.codecCtx);
        m_audioPktDecoder.codecCtx = nullptr;
    }
    if(m_videoPktDecoder.codecCtx != nullptr){
        avcodec_free_context(&m_videoPktDecoder.codecCtx);
        m_videoPktDecoder.codecCtx = nullptr;
    }
}

void Decoder::seekTo(int32_t target)
{
    if(m_isSeek) return; // 上次跳转未完成, 此次不予处理
    m_seekTarget = target;
    m_isSeek = true;
}

void Decoder::clearQueueCache()
{
    {
        std::lock_guard<std::mutex> lock(m_audioPktQueue.mutex);
        while(m_audioPktQueue.size > 0){
            av_packet_unref(&m_audioPktQueue.pktVec[m_audioPktQueue.readIndex].pkt);
//          AVPacket *packet = &m_audioPktQueue.pktVec[m_audioPktQueue.readIndex].pkt;
//          av_packet_unref(packet);
//          av_packet_free(&packet);
            m_audioPktQueue.readIndex = (m_audioPktQueue.readIndex + 1) % m_maxPktQueueSize;
            m_audioPktQueue.size--;
        }
    }
    {
        std::lock_guard<std::mutex> lock(m_videoPktQueue.mutex);
        while(m_videoPktQueue.size > 0){
            av_packet_unref(&m_videoPktQueue.pktVec[m_videoPktQueue.readIndex].pkt);
//          AVPacket *packet = &m_videoPktQueue.pktVec[m_videoPktQueue.readIndex].pkt;
//          av_packet_unref(packet);
//          av_packet_free(&packet);
            m_videoPktQueue.readIndex = (m_videoPktQueue.readIndex + 1) % m_maxPktQueueSize;
            m_videoPktQueue.size--;
        }
    }
    {
        std::lock_guard<std::mutex> lock(m_audioFrameQueue.mutex);
        while(m_audioFrameQueue.size > 0){
            av_frame_unref(&m_audioFrameQueue.frameVec[m_audioFrameQueue.readIndex].frame);
//          AVFrame *frame = &m_audioFrameQueue.frameVec[m_audioFrameQueue.readIndex].frame;
//          av_frame_unref(frame);
//          av_frame_free(&frame);
            m_audioFrameQueue.readIndex = (m_audioPktQueue.readIndex + 1) % m_maxFrameQueueSize;
            m_audioFrameQueue.size--;
        }
    }
    {
        std::lock_guard<std::mutex> lock(m_videoFrameQueue.mutex);
        while(m_videoFrameQueue.size > 0){
            av_frame_unref(&m_videoFrameQueue.frameVec[m_videoFrameQueue.readIndex].frame);
//          AVFrame *frame = &m_videoFrameQueue.frameVec[m_videoFrameQueue.readIndex].frame;
//          av_frame_unref(frame);
//          av_frame_free(&frame);
            m_videoPktQueue.readIndex = (m_videoFrameQueue.readIndex + 1) % m_maxFrameQueueSize;
            m_videoFrameQueue.size--;
        }
    }
}

bool Decoder::decode(const QString &url) //从视频内部获取信息填充成员变量
{

    initVal();
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

    // get context
    errorNum = avformat_find_stream_info(m_pAvFormatCtx, nullptr);
    if(errorNum < 0){
        av_strerror(errorNum, m_errBuf, sizeof(m_errBuf));
        QLOG_ERROR() << "avformat_find_stream_info fail: " << m_errBuf;
        av_dict_free(&fmtOpt);
        return false;
    }

    // get duration
    AVRational ratio = {1, AV_TIME_BASE}; // 1 / 1000000
    m_duration = (uint32_t)m_pAvFormatCtx->duration * av_q2d(ratio);
    av_dict_free(&fmtOpt);

    //get index
    m_videoIndex = av_find_best_stream(m_pAvFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if(m_videoIndex < 0){
        QLOG_ERROR() << "url no video stream";
        return false;
    }
    m_audioIndex = av_find_best_stream(m_pAvFormatCtx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if(m_audioIndex < 0){
        QLOG_ERROR() << "url no audio stream";
        return false;
    }

    // get decoder
    const AVCodec *audioCodec = avcodec_find_decoder(m_pAvFormatCtx->streams[m_audioIndex]->codecpar->codec_id);
    if(!audioCodec){
        QLOG_ERROR() << "avcodec_find_decoder audio fail";
        return false;
    }
    m_audioPktDecoder.codecCtx = avcodec_alloc_context3(audioCodec);
    if(!m_audioPktDecoder.codecCtx){
        QLOG_ERROR() << "avcodec_alloc_context3 audio fail";
        return false;
    }
    errorNum = avcodec_parameters_to_context(m_audioPktDecoder.codecCtx, m_pAvFormatCtx->streams[m_audioIndex]->codecpar);
    if(errorNum < 0){
        av_strerror(errorNum, m_errBuf, sizeof(m_errBuf));
        QLOG_ERROR() << "avcodec_parameters_to_context audio fail" << m_errBuf;
        return false;
    }
    errorNum = avcodec_open2(m_audioPktDecoder.codecCtx, audioCodec, nullptr);
    if(errorNum < 0){
        av_strerror(errorNum, m_errBuf, sizeof(m_errBuf));
        QLOG_ERROR() << "avcodec_open2 audio fail" << m_errBuf;
        return false;
    }

    const AVCodec *videoCodec = avcodec_find_decoder(m_pAvFormatCtx->streams[m_videoIndex]->codecpar->codec_id);
    if(!videoCodec){
        QLOG_ERROR() << "avcodec_find_decoder video fail";
        return false;
    }
    m_videoPktDecoder.codecCtx = avcodec_alloc_context3(videoCodec);
    if(!m_videoPktDecoder.codecCtx){
        QLOG_ERROR() << "avcodec_alloc_context3 video fail";
        return false;
    }
    errorNum = avcodec_parameters_to_context(m_videoPktDecoder.codecCtx, m_pAvFormatCtx->streams[m_videoIndex]->codecpar);
    if(errorNum < 0){
        av_strerror(errorNum, m_errBuf, sizeof(m_errBuf));
        QLOG_ERROR() << "avcodec_parameters_to_context video fail" << m_errBuf;
        return false;
    }
    errorNum = avcodec_open2(m_videoPktDecoder.codecCtx, videoCodec, nullptr);
    if(errorNum < 0){
        av_strerror(errorNum, m_errBuf, sizeof(m_errBuf));
        QLOG_ERROR() << "avcodec_open2 video fail" << m_errBuf;
        return false;
    }

    //get frame rate
    m_videoFrameRate = av_guess_frame_rate(m_pAvFormatCtx, m_pAvFormatCtx->streams[m_videoIndex], nullptr);

    // get packet
    ThreadPool::instance().commitTask([this](){
        this->demux();
    });
    // get frame
    ThreadPool::instance().commitTask([this](){
        this->audioDecode();
    });
    ThreadPool::instance().commitTask([this](){
        this->videoDecode();
    });

    return true;
}

void Decoder::demux()
{
    int errNum = -1;
    AVPacket *pkt = av_packet_alloc();

    while(true){
        if(m_exit.load()) break;
        if(m_audioPktQueue.size >= m_maxPktQueueSize || m_videoPktQueue.size >= m_maxPktQueueSize){
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }
        if(m_isSeek){
            int64_t target = m_seekTarget * AV_TIME_BASE;
            errNum = av_seek_frame(m_pAvFormatCtx, -1, target, AVSEEK_FLAG_BACKWARD);
            if(errNum < 0){
                av_strerror(errNum, m_errBuf, sizeof(m_errBuf));
                QLOG_ERROR() << "av_seek_frame fail" << m_errBuf;
            }
            else{
                packetQueueFlush(&m_audioPktQueue);
                packetQueueFlush(&m_videoPktQueue);
                m_audSeek = true;
                m_vidSeek = true;
            }
            m_isSeek = false;
        }

        errNum = av_read_frame(m_pAvFormatCtx, pkt);
        if(errNum == AVERROR_EOF){
            av_packet_free(&pkt);
            QLOG_INFO() << "read file end, success";
            break;
        }
        else if(errNum < 0){
            av_packet_free(&pkt);
            av_strerror(errNum, m_errBuf, sizeof(m_errBuf));
            QLOG_ERROR() << "av_read_frame fail" << m_errBuf;
            break;
        }

        if(pkt->stream_index == m_audioIndex){
            pushPacket(&m_audioPktQueue, pkt);
        }
        else if(pkt->stream_index == m_videoIndex){
            pushPacket(&m_videoPktQueue, pkt);
        }
        else{
            av_packet_unref(pkt);
        }
    }

    av_packet_free(&pkt);
    if(!m_exit.load()){
        while(m_audioFrameQueue.size > 0){
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        exit();
    }
    QLOG_INFO() << "demux thread exit";
}

void Decoder::audioDecode()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    AVPacket *pkt = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();

    while(true){
        if(m_exit.load()) break;
        if(m_audioFrameQueue.size >= m_maxFrameQueueSize){
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        int errNum = getPacket(&m_audioPktQueue, pkt, &m_audioPktDecoder);
        if(errNum){
            errNum = avcodec_send_packet(m_audioPktDecoder.codecCtx, pkt);
            av_packet_unref(pkt);
            if(errNum < 0){
                av_strerror(errNum, m_errBuf, sizeof(m_errBuf));
                QLOG_ERROR() << "avcodec_send_packet audio fail" << m_errBuf;
                continue;
            }
            while(true){
                errNum = avcodec_receive_frame(m_audioPktDecoder.codecCtx, frame);
                if(errNum == 0){ //sucess
                    if(m_audSeek){
                        int pts = (int)frame->pts * av_q2d(m_pAvFormatCtx->streams[m_audioIndex]->time_base);
                        if(pts < m_seekTarget){
                            av_frame_unref(frame);
                            continue;
                        }
                        else{
                            m_audSeek = 0;
                        }
                    }
                    pushAFrame(frame);
                }
                else{
                    break;
                }
            }
        }
        else{
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
    av_packet_free(&pkt);
    av_frame_free(&frame);
    QLOG_INFO() << "audioDecode thread exit";
}

void Decoder::videoDecode()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    AVPacket *pkt = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    while(true){
        if(m_exit.load()) break;
        if(m_videoFrameQueue.size >= m_maxFrameQueueSize){
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        int errNum = getPacket(&m_videoPktQueue, pkt, &m_videoPktDecoder);
        if(errNum){
            errNum = avcodec_send_packet(m_videoPktDecoder.codecCtx, pkt);
            av_packet_unref(pkt);
            if(errNum < 0 || errNum == AVERROR(EAGAIN) || errNum == AVERROR_EOF){
                av_strerror(errNum, m_errBuf, sizeof(m_errBuf));
                QLOG_ERROR() << "avcodec_send_packet video fail" << m_errBuf;
                continue;
            }
            while(true){
                errNum = avcodec_receive_frame(m_videoPktDecoder.codecCtx, frame);
                if(errNum == 0){
                    if(m_vidSeek){
                        int pts = (int)frame->pts * av_q2d(m_pAvFormatCtx->streams[m_videoIndex]->time_base);
                        if(pts < m_seekTarget){
                            av_frame_unref(frame);
                            continue;
                        }
                        else{
                            m_vidSeek = 0;
                        }
                    }
                    pushVFrame(frame);
                }
                else{
                    break;
                }
            }
        }
        else{
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
    av_packet_free(&pkt);
    av_frame_free(&frame);
    QLOG_INFO() << "video decode thread exit";
}

void Decoder::packetQueueFlush(FPacketQueue *queue)
{
    std::lock_guard<std::mutex> lock(queue->mutex);
    while(queue->size > 0){
        av_packet_unref(&queue->pktVec[queue->readIndex].pkt);
        queue->readIndex = (queue->readIndex + 1) % m_maxPktQueueSize;
        queue->size--;
    }
    queue->serial++;
}

void Decoder::pushPacket(FPacketQueue *queue, AVPacket *pkt)
{
    std::lock_guard<std::mutex> lock(queue->mutex);
    av_packet_move_ref(&queue->pktVec[queue->pushIndex].pkt, pkt);
    queue->pktVec[queue->pushIndex].serial = queue->serial;
    queue->pushIndex = (queue->pushIndex + 1) % m_maxPktQueueSize;
    queue->size++;
}

int Decoder::getPacket(FPacketQueue *queue, AVPacket *destPkt, FPktDecoder *decoder)
{
    std::unique_lock<std::mutex> lock(queue->mutex);
    while(queue->size == 0){
        bool ret = queue->cond.wait_for(lock, std::chrono::milliseconds(100), [&](){
            return queue->size > 0;
        });
        if(ret == false){
            return 0;
        }
    }
    if(decoder->serial != queue->serial){
        avcodec_flush_buffers(decoder->codecCtx);
        decoder->serial = queue->pktVec[queue->readIndex].serial;
        return 0;
    }
    av_packet_move_ref(destPkt, &queue->pktVec[queue->readIndex].pkt);
    decoder->serial = queue->pktVec[queue->readIndex].serial;
    queue->readIndex = (queue->readIndex + 1) % m_maxPktQueueSize;
    queue->size--;
    return true;
}

void Decoder::pushAFrame(AVFrame *frame)
{
    std::lock_guard<std::mutex> lock(m_audioFrameQueue.mutex);
    av_frame_move_ref(&m_audioFrameQueue.frameVec[m_audioFrameQueue.pushIndex].frame, frame);
    m_audioFrameQueue.frameVec[m_audioFrameQueue.pushIndex].serial = m_audioPktDecoder.serial;
    m_audioFrameQueue.pushIndex = (m_audioFrameQueue.pushIndex + 1) % m_maxFrameQueueSize;
    m_audioFrameQueue.size++;
}

void Decoder::pushVFrame(AVFrame *frame)
{
    std::lock_guard<std::mutex> lock(m_videoFrameQueue.mutex);
    m_videoFrameQueue.frameVec[m_videoFrameQueue.pushIndex].serial = m_videoPktDecoder.serial;
    m_videoFrameQueue.frameVec[m_videoFrameQueue.pushIndex].pts = frame->pts * av_q2d(m_pAvFormatCtx->streams[m_videoIndex]->time_base);
    m_videoFrameQueue.frameVec[m_videoFrameQueue.pushIndex].duration = m_videoFrameRate.den && m_videoFrameRate.num ? av_q2d(AVRational{m_videoFrameRate.num, m_videoFrameRate.den}) : 0.00;
    av_frame_move_ref(&m_videoFrameQueue.frameVec[m_videoFrameQueue.pushIndex].frame, frame);
    m_videoFrameQueue.pushIndex = (m_videoFrameQueue.pushIndex + 1) % m_maxFrameQueueSize;
    m_videoFrameQueue.size++;
}

int Decoder::getAFrame(AVFrame *frame)
{
    if(!frame) return 0;
    std::unique_lock<std::mutex> lock(m_audioFrameQueue.mutex);
    while(m_audioFrameQueue.size == 0){
        bool ret = m_audioFrameQueue.cond.wait_for(lock, std::chrono::milliseconds(100), [&](){
            return !m_exit.load() && m_audioFrameQueue.size;
        });
        if(!ret) return 0;
    }

    if(m_audioFrameQueue.frameVec[m_audioFrameQueue.readIndex].serial != m_audioPktQueue.serial){
        av_frame_unref(&m_audioFrameQueue.frameVec[m_audioFrameQueue.readIndex].frame);
        m_audioFrameQueue.readIndex = (m_audioFrameQueue.readIndex + 1) % m_maxFrameQueueSize;
        m_audioFrameQueue.size--;
        return 0;
    }
    av_frame_move_ref(frame, &m_audioFrameQueue.frameVec[m_audioFrameQueue.readIndex].frame);
    m_audioFrameQueue.readIndex = (m_audioFrameQueue.readIndex + 1) % m_maxFrameQueueSize;
    m_audioFrameQueue.size--;
    return 1;
}

int Decoder::getRemainingVFrameSize()
{
    std::lock_guard<std::mutex> lock(m_videoFrameQueue.mutex);
    if(m_videoFrameQueue.size == 0) return 0;
    return m_videoFrameQueue.size - m_videoFrameQueue.shown;
}

Decoder::FFrame *Decoder::getLastVFrame()
{
    FFrame *frame = &m_videoFrameQueue.frameVec[m_videoFrameQueue.readIndex];
    return frame;
}

Decoder::FFrame *Decoder::getVFrame()
{
    while(m_videoFrameQueue.size == 0){
        std::unique_lock<std::mutex> lock(m_videoFrameQueue.mutex);
        bool ret = m_videoFrameQueue.cond.wait_for(lock, std::chrono::milliseconds(100), [&](){
            return !m_exit.load() && m_videoFrameQueue.size;
        });
        if(ret == false) return nullptr;
    }
    int index = (m_videoFrameQueue.readIndex + m_videoFrameQueue.shown) % m_maxFrameQueueSize;
    FFrame *frame = &m_videoFrameQueue.frameVec[index];
    return frame;
}

Decoder::FFrame *Decoder::getNextVFrame()
{
    while(m_videoFrameQueue.size < 2){
        std::unique_lock<std::mutex> lock(m_videoFrameQueue.mutex);
        bool ret = m_videoFrameQueue.cond.wait_for(lock, std::chrono::milliseconds(100), [&](){
            return !m_exit.load() && m_videoFrameQueue.size >= 2;
        });
        if(ret == false) return nullptr;
    }
    int index = (m_videoFrameQueue.readIndex + m_videoFrameQueue.shown + 1) % m_maxFrameQueueSize;
    FFrame *frame = &m_videoFrameQueue.frameVec[index];
    return frame;
}

void Decoder::setNextVFrame()
{
    std::lock_guard<std::mutex> lock(m_videoFrameQueue.mutex);
    if(m_videoFrameQueue.size == 0) return;
    if(m_videoFrameQueue.shown == 0) m_videoFrameQueue.shown = 1;
    av_frame_unref(&m_videoFrameQueue.frameVec[m_videoFrameQueue.readIndex].frame);
    m_videoFrameQueue.readIndex = (m_videoFrameQueue.readIndex + 1) % m_maxFrameQueueSize;
    m_videoFrameQueue.size--;
}

