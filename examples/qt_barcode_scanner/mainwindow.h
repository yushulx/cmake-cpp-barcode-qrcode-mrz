#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QStatusBar>
#include <QtCore/QThread>
#include <QtCore/QMimeData>
#include <QtCore/QUrl>
#include <QtCore/QFileInfo>
#include <QtGui/QDragEnterEvent>
#include <QtGui/QDropEvent>
#include <QtGui/QCloseEvent>
#include <QtGui/QResizeEvent>
#include <QtGui/QPixmap>
#include <QtGui/QPainter>

#if ENABLE_CAMERA
#include <QtMultimedia/QCamera>
#include <QtMultimedia/QMediaCaptureSession>
#include <QtMultimedia/QMediaDevices>
#include <QtMultimediaWidgets/QVideoWidget>
#include <QtMultimedia/QVideoSink>
#include "videosurface.h"
#endif

#ifdef ENABLE_OPENCV_CAMERA
#include "opencvcamera.h"
#include <QtWidgets/QLabel>
#endif

#include "barcodeworker.h"

QT_BEGIN_NAMESPACE
namespace Ui
{
    class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override; // Handle window state changes

private slots:
    void openImage();
    void startCamera();
    void stopCamera();
    void setLicense();
    void loadTemplate();
    void clearResults();
    void about();
    void onBarcodeResults(const QList<BarcodeResult> &results);
    void onImageTabSelected();
    void onCameraTabSelected();
    void onLicenseStatusChanged(const QString &status, bool isValid);
    void testCameraDetection();
    void testCameraWithBackendForcing();

private:
    void setupConnections();
    void loadImageFile(const QString &filePath);
    void updateImageDisplay(const QPixmap &pixmap, const QList<BarcodeResult> &results = QList<BarcodeResult>());
    void updateResultsDisplay(const QList<BarcodeResult> &results);
    void updateLicenseStatus(const QString &status, bool isValid);

    // Camera helper methods
    bool tryStartQt6Camera(const QList<QCameraDevice> &cameras);
#ifdef ENABLE_OPENCV_CAMERA
    bool tryStartOpenCVCamera();
#endif

    // UI
    Ui::MainWindow *ui;

    // Camera components
#if ENABLE_CAMERA
    std::unique_ptr<QCamera> camera;
    std::unique_ptr<QMediaCaptureSession> captureSession;
    std::unique_ptr<QVideoWidget> videoWidget;
    VideoSurface *videoSurface;
#endif

#ifdef ENABLE_OPENCV_CAMERA
    OpenCVCamera *openCVCamera;
    QLabel *cameraLabel; // For displaying OpenCV frames
    bool useOpenCVCamera;
    QTimer *resizeTimer;      // Timer for handling resize events safely
    bool cameraUpdatesPaused; // Flag to pause camera updates during window transitions
#endif

    // Worker thread
    QThread *workerThread;
    BarcodeWorker *barcodeWorker;

    // Current image
    QPixmap currentPixmap;
    QList<BarcodeResult> currentImageResults;  // Store current image detection results
    QPixmap currentCameraFrame;                // Store current camera frame for overlay
    QList<BarcodeResult> currentCameraResults; // Store current camera detection results
    QString licenseKey;
    QString templateContent;
    QString lastImageDirectory; // Remember last used directory for image loading

    // Status bar
    QLabel *licenseStatusLabel;
};

#endif // MAINWINDOW_H
