#include "opencvcamera.h"

#ifdef ENABLE_OPENCV_CAMERA

#include <QDebug>
#include <QApplication>

OpenCVCamera::OpenCVCamera(QObject *parent)
    : QObject(parent), captureTimer(new QTimer(this)), active(false), currentCameraIndex(-1)
{
    connect(captureTimer, &QTimer::timeout, this, &OpenCVCamera::captureFrame);
    captureTimer->setInterval(33); // ~30 FPS
}

OpenCVCamera::~OpenCVCamera()
{
    stop();
}

bool OpenCVCamera::isAvailable() const
{
    // Try to detect any cameras
    cv::VideoCapture testCapture;
    for (int i = 0; i < 10; ++i)
    {
        if (testCapture.open(i))
        {
            testCapture.release();
            return true;
        }
    }
    return false;
}

QStringList OpenCVCamera::availableCameras() const
{
    QStringList cameras;
    cv::VideoCapture testCapture;

    for (int i = 0; i < 10; ++i)
    {
        if (testCapture.open(i))
        {
            cameras << QString("Camera %1").arg(i);
            testCapture.release();
        }
    }

    return cameras;
}

bool OpenCVCamera::start(int cameraIndex)
{
    if (active)
    {
        stop();
    }

    if (!capture.open(cameraIndex))
    {
        emit error(QString("Failed to open camera %1").arg(cameraIndex));
        return false;
    }

    // Set camera properties for better performance and proper aspect ratio
    // Try common resolutions that work well with the UI layout

    // First try 640x480 (4:3 aspect ratio) - most compatible
    capture.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    capture.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    capture.set(cv::CAP_PROP_FPS, 30);

    // Get actual resolution after setting
    int actualWidth = static_cast<int>(capture.get(cv::CAP_PROP_FRAME_WIDTH));
    int actualHeight = static_cast<int>(capture.get(cv::CAP_PROP_FRAME_HEIGHT));
    qDebug() << "Camera resolution set to:" << actualWidth << "x" << actualHeight;

    // If we got a different resolution, try to optimize it
    if (actualWidth != 640 || actualHeight != 480)
    {
        // Try 800x600 if available
        capture.set(cv::CAP_PROP_FRAME_WIDTH, 800);
        capture.set(cv::CAP_PROP_FRAME_HEIGHT, 600);

        actualWidth = static_cast<int>(capture.get(cv::CAP_PROP_FRAME_WIDTH));
        actualHeight = static_cast<int>(capture.get(cv::CAP_PROP_FRAME_HEIGHT));
        qDebug() << "Fallback camera resolution:" << actualWidth << "x" << actualHeight;
    }

    // Test if we can read a frame
    cv::Mat testFrame;
    if (!capture.read(testFrame) || testFrame.empty())
    {
        capture.release();
        emit error(QString("Camera %1 opened but cannot read frames").arg(cameraIndex));
        return false;
    }

    currentCameraIndex = cameraIndex;
    active = true;
    captureTimer->start();

    qDebug() << "OpenCV camera started successfully:" << cameraIndex;
    return true;
}

void OpenCVCamera::stop()
{
    if (active)
    {
        captureTimer->stop();
        capture.release();
        active = false;
        currentCameraIndex = -1;
        qDebug() << "OpenCV camera stopped";
    }
}

bool OpenCVCamera::isActive() const
{
    return active;
}

QImage OpenCVCamera::getCurrentFrame() const
{
    QMutexLocker locker(&frameMutex);
    return currentImage;
}

QPixmap OpenCVCamera::getCurrentPixmap() const
{
    QMutexLocker locker(&frameMutex);
    return QPixmap::fromImage(currentImage);
}

void OpenCVCamera::captureFrame()
{
    if (!active || !capture.isOpened())
    {
        return;
    }

    cv::Mat frame;
    if (!capture.read(frame) || frame.empty())
    {
        emit error("Failed to capture frame from camera");
        return;
    }

    // Convert BGR to RGB (OpenCV uses BGR by default)
    cv::Mat rgbFrame;
    cv::cvtColor(frame, rgbFrame, cv::COLOR_BGR2RGB);

    QMutexLocker locker(&frameMutex);
    currentMat = rgbFrame.clone();
    currentImage = matToQImage(rgbFrame);
    locker.unlock();

    // Emit signals
    emit frameReady(currentImage);
    emit frameReady(QPixmap::fromImage(currentImage));
}

QImage OpenCVCamera::matToQImage(const cv::Mat &mat)
{
    switch (mat.type())
    {
    case CV_8UC3: // 8-bit, 3 channels (RGB)
        return QImage(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_RGB888);
    case CV_8UC1: // 8-bit, 1 channel (grayscale)
        return QImage(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_Grayscale8);
    case CV_8UC4: // 8-bit, 4 channels (RGBA)
        return QImage(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_RGBA8888);
    default:
        qWarning() << "OpenCVCamera::matToQImage() - Mat format not supported:" << mat.type();
        return QImage();
    }
}

#endif // ENABLE_OPENCV_CAMERA
