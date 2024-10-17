#include "AVPlayer.h"

AVPlayer::AVPlayer(QObject *parent)
    :QObject(parent) ,
      m_decoder(new Decoder)
{

}

AVPlayer::~AVPlayer()
{

}

bool AVPlayer::play(const QString& url)
{
    return m_decoder->decode(url);
}
