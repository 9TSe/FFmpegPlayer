#include "AVPlayer.h"
#include "MsgBox.h"
#include "ThreadPool.h"
#include <QFileInfo>
#include <QsLog.h>

AVPlayer::AVPlayer(QObject *parent)
    :QObject(parent) ,
      m_decoder(new Decoder),
      m_duration(0),
      m_pause(false),
      m_exit(false),
      m_audioBuf(nullptr),
      m_fmtCtx(nullptr),
      m_swsCtx(nullptr),
      m_swrCtx(nullptr),
      m_volume(50),
      m_videoBuf(nullptr)
{
    m_audioFrame = av_frame_alloc();
}

AVPlayer::~AVPlayer()
{
    if(m_audioFrame){
        av_frame_free(&m_audioFrame);
    }
    initPlayer();
    if(m_decoder){
        delete m_decoder;
        m_decoder = nullptr;
    }
    if(m_swrCtx){
        swr_free(&m_swrCtx);
    }
    if(m_swsCtx){
        sws_freeContext(m_swsCtx);
    }
    if(m_audioBuf){
        av_free(m_audioBuf);
    }
    if(m_videoBuf){
        av_free(m_videoBuf);
    }
}

void AVPlayer::initPlayer()
{
    if(getState() != AVPlayer::AV_STOPPED){
        m_exit = true;
        if(getState() == AVPlayer::AV_PLAYING){
            SDL_PauseAudio(1); //传入1停止音频
        }
        m_decoder->exit();
        SDL_CloseAudio();
        if(m_swrCtx){
            swr_free(&m_swrCtx);
        }
        if(m_swsCtx){
            sws_freeContext(m_swsCtx);
        }
        m_swrCtx = nullptr;
        m_swsCtx = nullptr;
    }
}

AVPlayer::PlayState AVPlayer::getState()
{
    AVPlayer::PlayState state;
    switch (SDL_GetAudioStatus())
    {
    case SDL_AUDIO_PLAYING:
        state = AVPlayer::AV_PLAYING;
        break;
    case SDL_AUDIO_PAUSED:
        state = AVPlayer::AV_PAUSED;
        break;
    case SDL_AUDIO_STOPPED:
        state = AVPlayer::AV_STOPPED;
        break;
    default:
        state = AVPlayer::AV_NONE;
        break;
    }
    return state;
}

bool AVPlayer::play(const QString& url)
{
    QFileInfo fileInfo(url);
    QString suffix = fileInfo.suffix().toLower();
    if(suffix != "mp4" && suffix != "mp3"){
        MsgBox::error(nullptr, QString("文件无效, 不支持当前格式"));
        QLOG_ERROR() << "文件格式不正确";
        return false;
    }
    initPlayer(); //复用
    if(!m_decoder->decode(url)){
        MsgBox::error(nullptr, QString("文件解析失败"));
        QLOG_ERROR() << "文件解析失败";
        return false;
    }

    m_duration = m_decoder->duraiton();
    emit durationChanged(m_duration);

    m_pause = false;
    m_clockInitFlag = false;

    if(!initSDL()){
        QLOG_ERROR() << "init SDL fail";
        return false;
    }

    initVideo();
    return true;
}


bool AVPlayer::initSDL()
{
    if(SDL_Init(SDL_INIT_AUDIO) != 0){
        QLOG_ERROR() << "sdl init audio fail";
        return false;
    }
    m_exit = false;
    m_audioBufSize = 0;
    m_audioBufIndex = 0;
    m_lastAudioPts = -1;
    m_audioCodecPar = m_decoder->auidoCodecPar();

    SDL_AudioSpec audioSpec;
    audioSpec.channels = m_audioCodecPar->ch_layout.nb_channels; // 音频通道数
    audioSpec.freq = m_audioCodecPar->sample_rate; // 采样率(44100
    audioSpec.format = AUDIO_S16SYS; // 音频格式, (16bits
    audioSpec.silence = 0; // 静音为0
    audioSpec.userdata = this; // 用户数据
    audioSpec.samples = m_audioCodecPar->frame_size; // 每个音频帧的样本数量(1024
    audioSpec.callback = fillAudioStreamCallback;

    if(SDL_OpenAudio(&audioSpec, nullptr) < 0){
        QLOG_ERROR() << "SDL_OpenAudio fail";
        return false;
    }


    m_fmtCtx = m_decoder->formatContext();
    m_audioIndex = m_decoder->audioIndex();
    m_targetSampleFmt = AV_SAMPLE_FMT_S16; //16bits 位深
    m_targetChannels = m_audioCodecPar->ch_layout.nb_channels;
    m_targetFreq = m_audioCodecPar->sample_rate;
    m_targetNbSamples = m_audioCodecPar->frame_size; // 每个音频帧的数量(1024
    av_channel_layout_default(&m_targetChannelLayout, m_targetChannels);

    SDL_PauseAudio(0);
    return true;
}

// len = samples × channels × bytes_per_sample(16bits = 2bytes)
// len = 1024 x 2 x 2
void AVPlayer::fillAudioStreamCallback(void *userData, uint8_t *stream, int len)
{
    memset(stream, 0, len);
    AVPlayer *player = (AVPlayer*)userData;
    double audioPts = 0.00;

    while(len > 0){
        if(player->m_exit) break;
        if(player->m_audioBufIndex >= player->m_audioBufSize){
            int ret = player->m_decoder->getAFrame(player->m_audioFrame);
            if(ret)
            {
                player->m_audioBufIndex = 0;
                if((player->m_targetSampleFmt != player->m_audioFrame->format ||
                        player->m_targetFreq != player->m_audioFrame->sample_rate ||
                        player->m_targetNbSamples != player->m_audioFrame->nb_samples
                        ||!AVPlayer::compareChannelLayouts(&player->m_targetChannelLayout, &player->m_audioFrame->ch_layout)
                        )&& !player->m_swrCtx)
//                if((player->m_targetSampleFmt != player->m_audioFrame->format ||
//                        player->m_targetFreq != player->m_audioFrame->sample_rate ||
//                        player->m_targetNbSamples != player->m_audioFrame->nb_samples)
//                        && !player->m_swrCtx)
                {
                    ret = swr_alloc_set_opts2(&player->m_swrCtx,
                    &player->m_targetChannelLayout, player->m_targetSampleFmt, player->m_targetFreq,
                    &player->m_audioFrame->ch_layout, (enum AVSampleFormat)player->m_audioFrame->format,
                    player->m_audioFrame->sample_rate, 0, nullptr);

                    if(ret < 0 || swr_init(player->m_swrCtx)){
                        QLOG_ERROR() << "swr_alloc_set_opt2 OR swr_init fail";
                        return;
                    }
                }
                if(player->m_swrCtx){
                    //// data[0]代表某个(左)声道的信息
                    const uint8_t **datas = (const uint8_t**)player->m_audioFrame->extended_data;
                    // 每个通道期望的样本数量(1024) +256提供足够空间
                    int outSampleCount = (uint64_t)player->m_audioFrame->nb_samples *
                            (player->m_targetFreq / player->m_audioFrame->sample_rate) + 256;
                    // 缓冲区所需要的大小, 返回单位为字节
                    int outSize = av_samples_get_buffer_size(nullptr,
                                                             player->m_targetChannelLayout.nb_channels,
                                                             outSampleCount, player->m_targetSampleFmt, 0);
                    if(outSize < 0){
                        QLOG_ERROR() << "av_samples_get_buffer_size fail";
                        return;
                    }
                    // @param 2: 跟踪, 分配成功后, 更新为分配的大小
                    // @param 3: mini size
                    av_fast_malloc(&player->m_audioBuf, &player->m_audioBufSize, outSize);
                    if(!player->m_audioBuf){
                        QLOG_ERROR() << "av_fast_malloc fail";
                        return;
                    }
                    /** 音频重采样和格式转换
                     *  @param outSampleCount 输出样本数量
                     *  @param datas 输入指针
                     *  @param nb_samples 输入样本数量
                     *  @return 返回成功转换后的样本数量
                     */
                    int sampleNum = swr_convert(player->m_swrCtx, &player->m_audioBuf, outSampleCount,
                                                datas, player->m_audioFrame->nb_samples);
                    if(sampleNum < 0){
                        QLOG_ERROR() << "swr_convert fail";
                        return;
                    }
                    // 实际分配出的大小
                    player->m_audioBufSize = av_samples_get_buffer_size(nullptr, player->m_targetChannels,
                                                                sampleNum, player->m_targetSampleFmt, 0);
                }
                else{
                    // 此时不需要转换, 计算原音频所需要分配的空间大小
                    player->m_audioBufSize = av_samples_get_buffer_size(nullptr, player->m_targetChannels,
                                        player->m_audioFrame->nb_samples, player->m_targetSampleFmt, 0);
                    av_fast_malloc(&player->m_audioBuf, &player->m_audioBufSize, player->m_audioBufSize + 256);
                    if(!player->m_audioBuf){
                        QLOG_ERROR() << "av_fast_malloc fail (else";
                        return;
                    }
                    // 此处对标上述swrconvert中的分配m_audioBuf
                    memcpy(player->m_audioBuf, player->m_audioFrame->data[0], player->m_audioBufSize);
                }
                // 下个音频播放的时间
                audioPts = player->m_audioFrame->pts * av_q2d(player->m_fmtCtx->streams[player->m_audioIndex]->time_base);
                av_frame_unref(player->m_audioFrame);
            }
            else{
                if(player->m_decoder->isExit()){
                    emit player->avTerminate();
                }
                return;
            }
        }
        int tmpLen = player->m_audioBufSize - player->m_audioBufIndex; //剩余的空间
        tmpLen = tmpLen < len ? tmpLen : len;
        /**
         * @brief SDL_MixAudio
         * @param 2 : src
         * @param 3 : size
         * @param 4: 混合的音量
         */
        SDL_MixAudio(stream, player->m_audioBuf + player->m_audioBufIndex, tmpLen, player->m_volume);
        len -= tmpLen;
        player->m_audioBufIndex += tmpLen;
        stream += tmpLen;
    }
    player->m_audioClock.setClock(audioPts);
    //发送时间戳变化信号,因为进度以整数秒单位变化展示，
    //所以大于一秒才发送，避免过于频繁的信号槽通信消耗性能
    uint32_t _pts = (uint32_t)audioPts;
    if(_pts != player->m_lastAudioPts){
        emit player->avPtsChanged(_pts);
        _pts = player->m_lastAudioPts;
    }

}


void AVPlayer::initVideo()
{
    m_frameTimer = 0.0;
    m_videoCodecPar = m_decoder->videoCodecPar();
    m_videoIndex = m_decoder->videoIndex();

    m_imageWidth = m_videoCodecPar->width;
    m_imageHeight = m_videoCodecPar->height;
    m_aspectRatio = m_imageWidth != 0 && m_imageHeight != 0 ? static_cast<float>(m_imageWidth) / static_cast<float>(m_imageHeight) : 1.0f;

    m_dstPixFmt = AV_PIX_FMT_YUV422P;
    m_swsFlag = SWS_BICUBIC;

    int bufSize = av_image_get_buffer_size(m_dstPixFmt, m_imageWidth, m_imageHeight, 1);
    m_videoBuf = (uint8_t*)av_realloc(m_videoBuf, bufSize * sizeof(uint8_t));
    /**
     * @brief av_image_fill_arrays 当前函数中用于初始化 m_pixels, m_pitch
     * @param m_pixels 指向存储每个平面（如 YUV 图像的 Y、U、V 分量）的数据指针的数组
     * @param m_pitch 指向每个平面的行大小（以字节为单位）的数组
     * @param m_videoBuf 传入的图像数据
     */
    av_image_fill_arrays(m_pixels, m_pitch, m_videoBuf, m_dstPixFmt, m_imageWidth, m_imageHeight, 1);

    ThreadPool::instance().commitTask([this](){
        this->videoCallback();
    });
}

void AVPlayer::videoCallback()
{
    double time = 0.00;
    double duration = 0.00;
    double delay = 0.00;
    if(m_clockInitFlag == false){
        initAVClock();
    }

    do
    {
        if(m_exit) break;
        if(m_pause){
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        if(m_decoder->getRemainingVFrameSize()){

        }


    }while(true);
}

void AVPlayer::initAVClock()
{
    m_audioClock.setClock(0.00);
    m_videoClock.setClock(0.00);
    m_clockInitFlag = true;
}

bool AVPlayer::compareChannelLayouts(const AVChannelLayout *layout1, const AVChannelLayout *layout2)
{
    // 比较通道顺序
    if (layout1->order != layout2->order) {
        return false;
    }
    // 比较通道数量
    if (layout1->nb_channels != layout2->nb_channels) {
        return false;
    }
    // 根据 order 比较具体的通道布局
    if (layout1->order == AV_CHANNEL_ORDER_NATIVE) {
        return layout1->u.mask == layout2->u.mask; // 比较掩码
    } else if (layout1->order == AV_CHANNEL_ORDER_CUSTOM) {
        for (int i = 0; i < layout1->nb_channels; i++) {
            if (layout1->u.map[i].id != layout2->u.map[i].id) {
                return false; // 比较每个通道的 id
            }
        }
    }
    return true; // 布局相同
}
