#ifndef AVPLAYER_H
#define AVPLAYER_H
#include <QObject>
#include "Decoder.h"

class AVPlayer : public QObject
{
    Q_OBJECT
public:

    explicit AVPlayer(QObject *parent = nullptr);
    ~AVPlayer();
    bool play(const QString& url);

private:
    Decoder *m_decoder;

    uint8_t *m_buffer;
    uint32_t m_imageWidth;
    uint32_t m_imageHeight;
};

#endif // AVPLAYER_H
