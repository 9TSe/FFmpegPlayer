#include "AVPlayer.h"
#include "MsgBox.h"
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
      m_buffer(nullptr)
{
    m_audioFrame = av_frame_alloc();
}

AVPlayer::~AVPlayer()
{

}

void AVPlayer::initPlayer()
{

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

    m_fmtCtx = m_decoder->formatContext();
    m_audioIndex = m_decoder->audioIndex();
    m_targetSampleFmt = AV_SAMPLE_FMT_S16; //16bits 位深
    m_targetChannels = m_audioCodecPar->ch_layout.nb_channels;
    m_targetFreq = m_audioCodecPar->sample_rate;
    m_targetNbSamples = m_audioCodecPar->frame_size; // 每个音频帧的数量(1024
    av_channel_layout_default(&m_targetChannelLayout, m_targetChannels);

    if(SDL_OpenAudio(&audioSpec, nullptr) < 0){
        QLOG_ERROR() << "SDL_OpenAudio fail";
        return false;
    }
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
//                if((player->m_targetSampleFmt != player->m_audioFrame->format ||
//                        player->m_targetFreq != player->m_audioFrame->sample_rate ||
//                        player->m_targetNbSamples != player->m_audioFrame->nb_samples ||
//                        !compareChannelLayouts(&player->m_targetChannelLayout, &player->m_audioFrame->ch_layout))
//                        && !player->m_swrCtx)
                if((player->m_targetSampleFmt != player->m_audioFrame->format ||
                        player->m_targetFreq != player->m_audioFrame->sample_rate ||
                        player->m_targetNbSamples != player->m_audioFrame->nb_samples)
                        && !player->m_swrCtx)
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
        stream += tmpLen;
    }

}
bool compareChannelLayouts(const AVChannelLayout *layout1, const AVChannelLayout *layout2)
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
