#ifndef OPENCVCAMERA_H
#define OPENCVCAMERA_H

#include <QObject>
#include <QTimer>
#include <QImage>
#include <QPixmap>
#include <QMutex>
#include <QThread>
#include <opencv2/opencv.hpp>

class OpenCVCamera : public QObject
{
    Q_OBJECT

public:
    explicit OpenCVCamera(QObject *parent = nullptr);
    ~OpenCVCamera();

    bool isAvailable() const;
    QStringList availableCameras() const;
    bool start(int cameraIndex = 0);
    void stop();
    bool isActive() const;

    QImage getCurrentFrame() const;
    QPixmap getCurrentPixmap() const;

signals:
    void frameReady(const QImage &image);
    void frameReady(const QPixmap &pixmap);
    void error(const QString &errorString);

private slots:
    void captureFrame();

private:
    cv::VideoCapture capture;
    QTimer *captureTimer;
    mutable QMutex frameMutex;
    cv::Mat currentMat;
    QImage currentImage;
    bool active;
    int currentCameraIndex;

    QImage matToQImage(const cv::Mat &mat);
};

#endif // OPENCVCAMERA_H
