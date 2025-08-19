#ifndef VIDEOSURFACE_H
#define VIDEOSURFACE_H

#include <QtCore/QObject>
#include <QtMultimedia/QVideoSink>
#include <QtMultimedia/QVideoFrame>
#include <QtGui/QImage>

class VideoSurface : public QVideoSink
{
    Q_OBJECT

public:
    explicit VideoSurface(QObject *parent = nullptr);

signals:
    void frameAvailable(const QImage &frame);

private slots:
    void onVideoFrameChanged(const QVideoFrame &frame);

private:
    QImage videoFrameToImage(const QVideoFrame &frame);
};

#endif // VIDEOSURFACE_H
