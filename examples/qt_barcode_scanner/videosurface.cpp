#include "videosurface.h"
#include <QtCore/QDebug>

VideoSurface::VideoSurface(QObject *parent)
    : QVideoSink(parent)
{
    connect(this, &QVideoSink::videoFrameChanged, this, &VideoSurface::onVideoFrameChanged);
}

void VideoSurface::onVideoFrameChanged(const QVideoFrame &frame)
{
    if (frame.isValid())
    {
        QImage image = videoFrameToImage(frame);
        if (!image.isNull())
        {
            emit frameAvailable(image);
        }
    }
}

QImage VideoSurface::videoFrameToImage(const QVideoFrame &frame)
{
    if (!frame.isValid())
    {
        return QImage();
    }

    QVideoFrame clonedFrame(frame);
    if (!clonedFrame.map(QVideoFrame::ReadOnly))
    {
        return QImage();
    }

    QImage image;

    switch (clonedFrame.pixelFormat())
    {
    case QVideoFrameFormat::Format_RGBX8888:
        image = QImage(clonedFrame.bits(0),
                       clonedFrame.width(),
                       clonedFrame.height(),
                       clonedFrame.bytesPerLine(0),
                       QImage::Format_RGB888);
        break;
    case QVideoFrameFormat::Format_ARGB8888:
        image = QImage(clonedFrame.bits(0),
                       clonedFrame.width(),
                       clonedFrame.height(),
                       clonedFrame.bytesPerLine(0),
                       QImage::Format_ARGB32);
        break;
    case QVideoFrameFormat::Format_BGRA8888:
        image = QImage(clonedFrame.bits(0),
                       clonedFrame.width(),
                       clonedFrame.height(),
                       clonedFrame.bytesPerLine(0),
                       QImage::Format_ARGB32);
        break;
    case QVideoFrameFormat::Format_YUV420P:
    case QVideoFrameFormat::Format_NV12:
    case QVideoFrameFormat::Format_NV21:
        // Convert YUV to RGB
        // This is a simplified conversion; in practice, you might want more sophisticated handling
        {
            int width = clonedFrame.width();
            int height = clonedFrame.height();
            image = QImage(width, height, QImage::Format_RGB888);

            const uchar *yData = clonedFrame.bits(0);
            const uchar *uData = clonedFrame.bits(1);
            const uchar *vData = clonedFrame.bits(2);

            int yStride = clonedFrame.bytesPerLine(0);
            int uvStride = clonedFrame.bytesPerLine(1);

            for (int y = 0; y < height; ++y)
            {
                uchar *rgbLine = image.scanLine(y);
                for (int x = 0; x < width; ++x)
                {
                    int yValue = yData[y * yStride + x];
                    int uValue = uData[(y / 2) * uvStride + (x / 2)] - 128;
                    int vValue = vData[(y / 2) * uvStride + (x / 2)] - 128;

                    // YUV to RGB conversion
                    int r = qBound(0, static_cast<int>(yValue + (1.402 * vValue)), 255);
                    int g = qBound(0, static_cast<int>(yValue - (0.344 * uValue) - (0.714 * vValue)), 255);
                    int b = qBound(0, static_cast<int>(yValue + (1.772 * uValue)), 255);

                    rgbLine[x * 3] = r;
                    rgbLine[x * 3 + 1] = g;
                    rgbLine[x * 3 + 2] = b;
                }
            }
        }
        break;
    default:
        // For other formats, try to convert using Qt's built-in conversion
        image = clonedFrame.toImage();
        if (image.format() != QImage::Format_RGB888)
        {
            image = image.convertToFormat(QImage::Format_RGB888);
        }
        break;
    }

    clonedFrame.unmap();

    return image;
}
