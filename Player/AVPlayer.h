#ifndef AVPLAYER_H
#define AVPLAYER_H
#include <QObject>

class AVPlayer : public QObject
{
    Q_OBJECT
public:

    explicit AVPlayer(QObject *parent = nullptr);
    ~AVPlayer();
};

#endif // AVPLAYER_H
