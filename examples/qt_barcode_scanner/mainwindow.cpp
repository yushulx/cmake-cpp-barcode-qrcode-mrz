#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QtWidgets/QScrollArea>
#include <QtCore/QStandardPaths>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtGui/QImageReader>
#include <QtCore/QCoreApplication>
#include <QtCore/QLibraryInfo>
#include <QtCore/QPluginLoader>
#include <QtCore/QTimer>
#include <QtCore/QDebug>

#ifdef ENABLE_OPENCV_CAMERA
#include <opencv2/opencv.hpp>
#endif

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), camera(nullptr), captureSession(nullptr), videoWidget(nullptr), videoSurface(nullptr), workerThread(nullptr), barcodeWorker(nullptr), licenseKey("LICENSE-KEY"), lastImageDirectory(QStandardPaths::writableLocation(QStandardPaths::PicturesLocation))
#ifdef ENABLE_OPENCV_CAMERA
      ,
      openCVCamera(nullptr), cameraLabel(nullptr), useOpenCVCamera(false), resizeTimer(nullptr), cameraUpdatesPaused(false)
#endif
{
    ui->setupUi(this);
    setAcceptDrops(true);

    setupConnections();

    // Setup Qt6 video widget for camera tab
    // Don't create a new one - use the existing one from UI and convert it
    QWidget *existingVideoWidget = ui->videoWidget;
    ui->cameraLayout->removeWidget(existingVideoWidget);
    existingVideoWidget->deleteLater();

    // Create new QVideoWidget and add it to the layout
    videoWidget = std::make_unique<QVideoWidget>();
    videoWidget->setMinimumSize(580, 400);
    videoWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    videoWidget->setAspectRatioMode(Qt::KeepAspectRatio);
    videoWidget->setStyleSheet("QVideoWidget { background-color: #000; border: 1px solid #333; }");
    ui->cameraLayout->insertWidget(0, videoWidget.get());

#ifdef ENABLE_OPENCV_CAMERA
    // Setup OpenCV camera label (initially hidden)
    cameraLabel = new QLabel();
    cameraLabel->setMinimumSize(580, 400);
    cameraLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    cameraLabel->setStyleSheet("border: 1px solid gray; background-color: black;");
    cameraLabel->setAlignment(Qt::AlignCenter);
    cameraLabel->setText("OpenCV Camera View");
    cameraLabel->setScaledContents(false); // Important: don't use setScaledContents(true)
    cameraLabel->hide();                   // Hidden initially
    ui->cameraLayout->insertWidget(0, cameraLabel);

    // Create OpenCV camera
    openCVCamera = new OpenCVCamera(this);
    connect(openCVCamera, QOverload<const QPixmap &>::of(&OpenCVCamera::frameReady), this, [this](const QPixmap &pixmap)
            {
        if (useOpenCVCamera && cameraLabel && cameraLabel->isVisible() && !cameraUpdatesPaused) {
            // Store current camera frame for overlay
            currentCameraFrame = pixmap;
            
            // Get the current label size
            QSize labelSize = cameraLabel->size();
            
            // Only scale if we have a valid size
            if (labelSize.width() > 10 && labelSize.height() > 10) {
                // Check if we have current barcode results to overlay
                if (!currentCameraResults.isEmpty()) {
                    // Create overlay on the current frame
                    QPixmap overlayPixmap = pixmap;
                    QPainter painter(&overlayPixmap);
                    painter.setRenderHint(QPainter::Antialiasing);

                    // Draw barcode detection boxes
                    for (const auto &result : currentCameraResults) {
                        if (!result.points.isEmpty() && result.points.size() >= 4) {
                            QPen pen(Qt::green, 3);
                            painter.setPen(pen);

                            // Draw polygon connecting all points
                            QPolygonF polygon;
                            for (const auto &point : result.points) {
                                polygon << point;
                            }
                            painter.drawPolygon(polygon);

                            // Draw format text
                            if (!result.format.isEmpty()) {
                                painter.setPen(QPen(Qt::yellow, 2));
                                QFont font = painter.font();
                                font.setPointSize(12);
                                font.setBold(true);
                                painter.setFont(font);
                                
                                // Position text near first point
                                QPointF textPos = result.points.first();
                                textPos.setY(textPos.y() - 10); // Above the barcode
                                painter.drawText(textPos, result.format);
                            }
                        }
                    }
                    painter.end();

                    // Scale the pixmap with overlay to fit within the label
                    QPixmap scaledPixmap = overlayPixmap.scaled(
                        labelSize, 
                        Qt::KeepAspectRatio, 
                        Qt::SmoothTransformation
                    );
                    cameraLabel->setPixmap(scaledPixmap);
                } else {
                    // No barcode results, just scale the original pixmap
                    QPixmap scaledPixmap = pixmap.scaled(
                        labelSize, 
                        Qt::KeepAspectRatio, 
                        Qt::SmoothTransformation
                    );
                    cameraLabel->setPixmap(scaledPixmap);
                }
            }
            
            // Process frame for barcode detection (continue even when paused for detection)
            if (barcodeWorker) {
                QImage image = pixmap.toImage();
                barcodeWorker->processImage(image);
            }
        } });

    connect(openCVCamera, &OpenCVCamera::error, this, [this](const QString &error)
            { ui->resultsTextEdit->append(QString("OpenCV Camera Error: %1").arg(error)); });

    // Initialize resize timer
    resizeTimer = new QTimer(this);
    resizeTimer->setSingleShot(true);
    resizeTimer->setInterval(100); // 100ms delay

    connect(resizeTimer, &QTimer::timeout, this, [this]()
            {
        // Update OpenCV camera display when window is resized
        // Add extensive safety checks to prevent crashes
        if (!this || !ui || cameraUpdatesPaused) return; // Safety check for valid object and paused state
        
        // Use QMetaObject::invokeMethod to ensure thread safety
        QMetaObject::invokeMethod(this, [this]() {
            if (useOpenCVCamera && cameraLabel && openCVCamera && !cameraUpdatesPaused) {
                // Check if objects are still valid and camera is actually running
                if (!cameraLabel->isVisible()) return;
                
                // Check camera state in a thread-safe way
                bool isActive = false;
                try {
                    isActive = openCVCamera->isActive();
                } catch (...) {
                    return; // Camera in invalid state, skip update
                }
                
                if (!isActive) return;
                
                try {
                    // Get current frame in a safe way
                    QImage currentFrame = openCVCamera->getCurrentFrame();
                    if (!currentFrame.isNull()) {
                        QSize labelSize = cameraLabel->size();
                        if (labelSize.width() > 10 && labelSize.height() > 10) {
                            QPixmap scaledPixmap = QPixmap::fromImage(currentFrame).scaled(
                                labelSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                            
                            // Final safety check before setting pixmap
                            if (cameraLabel && !currentFrame.isNull() && !cameraUpdatesPaused) {
                                cameraLabel->setPixmap(scaledPixmap);
                            }
                        }
                    }
                } catch (...) {
                    // Ignore any exceptions during resize - camera might be transitioning
                    qDebug() << "Exception during camera resize update - camera transitioning";
                }
            }
        }, Qt::QueuedConnection); });
#endif

    // Setup worker thread
    workerThread = new QThread(this);
    barcodeWorker = new BarcodeWorker();
    barcodeWorker->moveToThread(workerThread);

    connect(workerThread, &QThread::started, barcodeWorker, &BarcodeWorker::initialize);
    connect(barcodeWorker, &BarcodeWorker::resultsReady, this, &MainWindow::onBarcodeResults);
    connect(barcodeWorker, &BarcodeWorker::licenseStatusChanged, this, &MainWindow::onLicenseStatusChanged);

    // Setup status bar
    licenseStatusLabel = new QLabel("License: Checking...");
    licenseStatusLabel->setMinimumWidth(200);
    ui->statusbar->addPermanentWidget(licenseStatusLabel);

    // Debug: List supported image formats
    QList<QByteArray> supportedFormats = QImageReader::supportedImageFormats();
    QStringList formatList;
    for (const QByteArray &format : supportedFormats)
    {
        formatList << QString(format);
    }
    qDebug() << "Supported image formats:" << formatList;

    workerThread->start();
}

MainWindow::~MainWindow()
{
    // Stop cameras first
    if (camera)
    {
        camera->stop();
    }

#ifdef ENABLE_OPENCV_CAMERA
    if (openCVCamera)
    {
        openCVCamera->stop();
    }

    // Stop and cleanup resize timer
    if (resizeTimer)
    {
        resizeTimer->stop();
        resizeTimer->deleteLater();
        resizeTimer = nullptr;
    }
#endif

    if (workerThread)
    {
        workerThread->quit();
        workerThread->wait();
    }

    delete ui;
}

void MainWindow::setupConnections()
{
    // Button connections
    connect(ui->loadImageButton, &QPushButton::clicked, this, &MainWindow::openImage);
    connect(ui->startCameraButton, &QPushButton::clicked, this, &MainWindow::startCamera);
    connect(ui->stopCameraButton, &QPushButton::clicked, this, &MainWindow::stopCamera);
    connect(ui->clearResultsButton, &QPushButton::clicked, this, &MainWindow::clearResults);

    // Menu connections
    connect(ui->actionOpen, &QAction::triggered, this, &MainWindow::openImage);
    connect(ui->actionExit, &QAction::triggered, this, &QWidget::close);
    connect(ui->actionSetLicense, &QAction::triggered, this, &MainWindow::setLicense);
    connect(ui->actionLoadTemplate, &QAction::triggered, this, &MainWindow::loadTemplate);
    connect(ui->actionAbout, &QAction::triggered, this, &MainWindow::about);

    // Tab widget connections
    connect(ui->tabWidget, &QTabWidget::currentChanged, this, [this](int index)
            {
        if (index == 0) onImageTabSelected();
        else if (index == 1) onCameraTabSelected(); });
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
    {
        QList<QUrl> urls = event->mimeData()->urls();
        if (!urls.isEmpty())
        {
            QString fileName = urls.first().toLocalFile();
            QString extension = QFileInfo(fileName).suffix().toLower();

            // List of supported image extensions
            QStringList supportedExtensions = {
                "jpg", "jpeg", "png", "bmp", "tiff", "tif", "gif", "webp", "ico", "svg"};

            if (supportedExtensions.contains(extension))
            {
                event->acceptProposedAction();
                return;
            }
            else
            {
                ui->resultsTextEdit->append(QString("Drag-and-drop: Unsupported file type '%1' for file: %2").arg(extension, QFileInfo(fileName).fileName()));
            }
        }
    }
    event->ignore();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    QList<QUrl> urls = event->mimeData()->urls();
    if (!urls.isEmpty())
    {
        QString fileName = urls.first().toLocalFile();
        loadImageFile(fileName);
        ui->tabWidget->setCurrentIndex(0); // Switch to image tab
        event->acceptProposedAction();
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (camera)
    {
        camera->stop();
    }
#ifdef ENABLE_OPENCV_CAMERA
    if (openCVCamera)
    {
        openCVCamera->stop();
    }
#endif
    event->accept();
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    // Always call parent first
    QMainWindow::resizeEvent(event);

    // Safety check - don't process resize events during destruction
    if (!this || !ui || cameraUpdatesPaused)
        return;

#ifdef ENABLE_OPENCV_CAMERA
    // Use member timer to defer camera update and avoid crashes
    if (resizeTimer && useOpenCVCamera && cameraLabel)
    {
        // Only restart timer if camera is actually running and visible
        // Add extra checks to prevent crashes during window state changes
        if (openCVCamera && !cameraUpdatesPaused)
        {
            try
            {
                bool isActive = openCVCamera->isActive();
                bool isVisible = cameraLabel->isVisible();

                if (isActive && isVisible && !isMinimized())
                {
                    // Stop any pending timer first
                    resizeTimer->stop();
                    // Start timer with longer delay to handle rapid resize events
                    resizeTimer->start(200); // Increased delay to 200ms
                }
            }
            catch (...)
            {
                // Camera might be in transition, skip resize update
                qDebug() << "Camera state check failed during resize - skipping update";
            }
        }
    }
#endif
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);

#ifdef ENABLE_OPENCV_CAMERA
    if (event->type() == QEvent::WindowStateChange)
    {
        // Temporarily pause camera updates during window state changes to prevent crashes
        cameraUpdatesPaused = true;

        // Stop any pending resize timer
        if (resizeTimer)
        {
            resizeTimer->stop();
        }

        // Use a single-shot timer to resume camera updates after window state change completes
        QTimer::singleShot(500, this, [this]()
                           {
            cameraUpdatesPaused = false;
            qDebug() << "Camera updates resumed after window state change"; });

        qDebug() << "Window state changed - camera updates paused temporarily";
    }
#endif
}

void MainWindow::openImage()
{
    // Create a comprehensive filter that includes all common image formats
    QString imageFilters =
        "All Supported Images (*.jpg *.jpeg *.png *.bmp *.tiff *.tif *.gif *.webp *.ico *.svg);;"
        "JPEG Images (*.jpg *.jpeg);;"
        "PNG Images (*.png);;"
        "BMP Images (*.bmp);;"
        "TIFF Images (*.tiff *.tif);;"
        "GIF Images (*.gif);;"
        "WebP Images (*.webp);;"
        "All Files (*.*)";

    QString fileName = QFileDialog::getOpenFileName(this,
                                                    "Open Image File",
                                                    lastImageDirectory,
                                                    imageFilters);

    if (!fileName.isEmpty())
    {
        // Remember the directory for next time
        lastImageDirectory = QFileInfo(fileName).absolutePath();

        loadImageFile(fileName);
        ui->tabWidget->setCurrentIndex(0); // Switch to image tab
    }
}

void MainWindow::startCamera()
{
    try
    {
        ui->resultsTextEdit->append("Initializing camera system...");

        // First try Qt6 Multimedia
        QList<QCameraDevice> cameras = QMediaDevices::videoInputs();
        ui->resultsTextEdit->append(QString("Qt6 detected cameras: %1").arg(cameras.size()));

        if (!cameras.isEmpty())
        {
            ui->resultsTextEdit->append("Attempting Qt6 Multimedia camera...");

            // Try Qt6 camera first
            if (tryStartQt6Camera(cameras))
            {
                return; // Success with Qt6
            }

            ui->resultsTextEdit->append("Qt6 camera failed, trying OpenCV fallback...");
        }
        else
        {
            ui->resultsTextEdit->append("No Qt6 cameras detected, checking OpenCV...");
        }

#ifdef ENABLE_OPENCV_CAMERA
        // Fallback to OpenCV camera
        if (tryStartOpenCVCamera())
        {
            return; // Success with OpenCV
        }
#endif

        // If we get here, both methods failed
        // Run comprehensive camera diagnostics
        testCameraDetection();

        // Also try backend forcing
        ui->resultsTextEdit->append("\n=== Trying Backend Forcing ===");
        testCameraWithBackendForcing();

        QMessageBox::information(this, "Camera Info",
                                 "No camera devices found with either Qt6 or OpenCV.\n\n"
                                 "Detailed diagnostic information has been displayed in the results area.\n\n"
                                 "You can still use the file scanning feature.");
    }
    catch (const std::exception &e)
    {
        QMessageBox::warning(this, "Camera Error",
                             QString("Failed to start camera: %1").arg(e.what()));
    }
}

bool MainWindow::tryStartQt6Camera(const QList<QCameraDevice> &cameras)
{
    try
    {
        // List available cameras for debugging
        ui->resultsTextEdit->append("Available Qt6 cameras:");
        for (const auto &cameraDevice : cameras)
        {
            ui->resultsTextEdit->append(QString("- %1 (%2)").arg(cameraDevice.description(), cameraDevice.id()));
        }

        // Use the default camera
        camera = std::make_unique<QCamera>(cameras.first());
        captureSession = std::make_unique<QMediaCaptureSession>();

        // Connect camera error signals
        connect(camera.get(), &QCamera::errorOccurred, this, [this](QCamera::Error error, const QString &errorString)
                { ui->resultsTextEdit->append(QString("Qt6 Camera error: %1").arg(errorString)); });

        // Connect active state changes
        connect(camera.get(), &QCamera::activeChanged, this, [this](bool active)
                { ui->resultsTextEdit->append(QString("Qt6 Camera active state: %1").arg(active ? "true" : "false")); });

        // Setup video surface for frame processing
        videoSurface = new VideoSurface(this);
        connect(videoSurface, &VideoSurface::frameAvailable, barcodeWorker, &BarcodeWorker::processFrame);

        captureSession->setCamera(camera.get());

        // In Qt6, we can set both video output and video sink
        captureSession->setVideoOutput(videoWidget.get());
        captureSession->setVideoSink(videoSurface);

        camera->start();

        // Show Qt6 video widget, hide OpenCV label
        videoWidget->show();
#ifdef ENABLE_OPENCV_CAMERA
        if (cameraLabel)
            cameraLabel->hide();
        useOpenCVCamera = false;
#endif

        ui->startCameraButton->setEnabled(false);
        ui->stopCameraButton->setEnabled(true);

        ui->resultsTextEdit->append(QString("Qt6 camera started successfully: %1").arg(cameras.first().description()));
        return true;
    }
    catch (const std::exception &e)
    {
        ui->resultsTextEdit->append(QString("Qt6 camera failed: %1").arg(e.what()));
        return false;
    }
}

#ifdef ENABLE_OPENCV_CAMERA
bool MainWindow::tryStartOpenCVCamera()
{
    if (!openCVCamera || !cameraLabel)
    {
        ui->resultsTextEdit->append("OpenCV camera not available (not compiled in)");
        return false;
    }

    if (!openCVCamera->isAvailable())
    {
        ui->resultsTextEdit->append("No OpenCV cameras detected");
        return false;
    }

    QStringList availableCameras = openCVCamera->availableCameras();
    ui->resultsTextEdit->append(QString("Available OpenCV cameras: %1").arg(availableCameras.join(", ")));

    // Try to start the first available camera
    if (openCVCamera->start(0))
    {
        // Hide Qt6 video widget, show OpenCV label
        videoWidget->hide();
        cameraLabel->show();
        useOpenCVCamera = true;

        ui->startCameraButton->setEnabled(false);
        ui->stopCameraButton->setEnabled(true);

        ui->resultsTextEdit->append("OpenCV camera started successfully!");
        return true;
    }
    else
    {
        ui->resultsTextEdit->append("Failed to start OpenCV camera");
        return false;
    }
}
#endif

void MainWindow::testCameraDetection()
{
    QStringList debugInfo;
    debugInfo << "=== Camera Detection Diagnostics ===";

    // Check Qt Multimedia capabilities
    debugInfo << QString("Qt Version: %1").arg(QT_VERSION_STR);
    debugInfo << QString("Qt Runtime Version: %1").arg(qVersion());

    // Check Qt paths and plugins
    debugInfo << QString("Qt Plugin Path: %1").arg(QLibraryInfo::path(QLibraryInfo::PluginsPath));
    debugInfo << QString("Application Dir: %1").arg(QCoreApplication::applicationDirPath());

    // Check for multimedia plugins
    QDir pluginsDir(QCoreApplication::applicationDirPath());
    pluginsDir.cd("platforms");
    debugInfo << QString("Platforms plugins: %1").arg(pluginsDir.exists() ? "FOUND" : "MISSING");

    pluginsDir = QDir(QCoreApplication::applicationDirPath());
    pluginsDir.cd("multimedia");
    debugInfo << QString("Multimedia plugins: %1").arg(pluginsDir.exists() ? "FOUND" : "MISSING");
    if (pluginsDir.exists())
    {
        QStringList plugins = pluginsDir.entryList(QStringList() << "*.dll", QDir::Files);
        debugInfo << QString("Available multimedia plugins: %1").arg(plugins.join(", "));
    }

    // Check for specific Windows Media Foundation support
    QDir wmfDir(QCoreApplication::applicationDirPath());
    wmfDir.cd("multimedia");
    bool hasWindowsPlugin = wmfDir.exists("windowsmediaplugin.dll");
    bool hasFFmpegPlugin = wmfDir.exists("ffmpegmediaplugin.dll");
    debugInfo << QString("Windows Media Foundation plugin: %1").arg(hasWindowsPlugin ? "FOUND" : "MISSING");
    debugInfo << QString("FFmpeg plugin: %1").arg(hasFFmpegPlugin ? "FOUND" : "MISSING");

    debugInfo << "";

    // List all available cameras
    QList<QCameraDevice> cameras = QMediaDevices::videoInputs();
    debugInfo << QString("Total cameras detected: %1").arg(cameras.size());

    if (cameras.isEmpty())
    {
        debugInfo << "";
        debugInfo << "=== OpenCV vs Qt Multimedia Comparison ===";
        debugInfo << "OpenCV typically uses:";
        debugInfo << "  - DirectShow (Windows native)";
        debugInfo << "  - Media Foundation (Windows 10+)";
        debugInfo << "  - DXVA (Direct3D Video Acceleration)";
        debugInfo << "";
        debugInfo << "Qt6 Multimedia uses:";
        debugInfo << "  - Windows Media Foundation (preferred)";
        debugInfo << "  - FFmpeg (fallback)";
        debugInfo << "  - DirectShow (legacy)";
        debugInfo << "";
        debugInfo << "Possible solutions:";
        debugInfo << "1. Set QT_MULTIMEDIA_PREFERRED_PLUGINS environment variable";
        debugInfo << "2. Force DirectShow backend (more compatible with OpenCV)";
        debugInfo << "3. Check Windows Media Foundation codec support";
    }

    for (int i = 0; i < cameras.size(); ++i)
    {
        const QCameraDevice &camera = cameras[i];
        debugInfo << QString("Camera %1:").arg(i + 1);
        debugInfo << QString("  - ID: %1").arg(camera.id());
        debugInfo << QString("  - Description: %1").arg(camera.description());
        debugInfo << QString("  - Default: %1").arg(camera.isDefault() ? "YES" : "NO");

        // Test camera creation
        QCamera *testCamera = new QCamera(camera);
        debugInfo << QString("  - Creation: %1").arg(testCamera ? "SUCCESS" : "FAILED");

        if (testCamera)
        {
            debugInfo << QString("  - Error State: %1").arg(testCamera->errorString());
            delete testCamera;
        }
    }

    // Windows-specific guidance
    debugInfo << "";
    debugInfo << "=== Windows Troubleshooting Steps ===";
    debugInfo << "1. Check Windows Camera Privacy Settings:";
    debugInfo << "   - Open Settings > Privacy & Security > Camera";
    debugInfo << "   - Enable 'Camera access' (system-wide)";
    debugInfo << "   - Enable 'Let apps access your camera'";
    debugInfo << "   - Enable 'Let desktop apps access your camera'";
    debugInfo << "";
    debugInfo << "2. Check Device Manager:";
    debugInfo << "   - Open Device Manager";
    debugInfo << "   - Look under 'Cameras' or 'Imaging devices'";
    debugInfo << "   - Ensure no yellow warning triangles";
    debugInfo << "   - Try updating camera drivers";
    debugInfo << "";
    debugInfo << "3. Test with Windows Camera app:";
    debugInfo << "   - Open Windows Camera app";
    debugInfo << "   - Verify camera works there first";
    debugInfo << "";
    debugInfo << "4. Antivirus/Security Software:";
    debugInfo << "   - Some security software blocks camera access";
    debugInfo << "   - Check antivirus camera protection settings";

    // Display all diagnostic information
    ui->resultsTextEdit->clear();
    ui->resultsTextEdit->append(debugInfo.join("\n"));
}

void MainWindow::testCameraWithBackendForcing()
{
    QStringList debugInfo;
    debugInfo << "=== Testing Qt Multimedia Backends ===";

    // Save current environment
    QStringList originalEnv;
    originalEnv << qgetenv("QT_MULTIMEDIA_PREFERRED_PLUGINS");
    originalEnv << qgetenv("QT_QPA_PLATFORM_PLUGIN_PATH");

    // Test different backends
    QStringList backends = {"windowsmediaplugin", "ffmpegmediaplugin", "directshow"};

    for (const QString &backend : backends)
    {
        debugInfo << QString("Testing backend: %1").arg(backend);

        // Set environment variable to force backend
        qputenv("QT_MULTIMEDIA_PREFERRED_PLUGINS", backend.toUtf8());

        // Try to detect cameras with this backend
        QList<QCameraDevice> cameras = QMediaDevices::videoInputs();
        debugInfo << QString("  Cameras found: %1").arg(cameras.size());

        if (!cameras.isEmpty())
        {
            debugInfo << "  Camera details:";
            for (const auto &camera : cameras)
            {
                debugInfo << QString("    - %1 (%2)").arg(camera.description(), camera.id());
            }

            // Try to create and start a camera
            QCamera *testCamera = new QCamera(cameras.first());
            if (testCamera)
            {
                debugInfo << "  Camera creation: SUCCESS";
                debugInfo << QString("  Camera error: %1").arg(testCamera->errorString());
                delete testCamera;
            }
        }
        debugInfo << "";
    }

    // Restore original environment
    if (!originalEnv[0].isEmpty())
    {
        qputenv("QT_MULTIMEDIA_PREFERRED_PLUGINS", originalEnv[0].toUtf8());
    }
    else
    {
        qunsetenv("QT_MULTIMEDIA_PREFERRED_PLUGINS");
    }

    debugInfo << "=== Alternative Solutions ===";
    debugInfo << "1. Try setting environment variable before launching:";
    debugInfo << "   set QT_MULTIMEDIA_PREFERRED_PLUGINS=windowsmediaplugin";
    debugInfo << "   QtBarcodeScanner.exe";
    debugInfo << "";
    debugInfo << "2. Or try DirectShow compatibility:";
    debugInfo << "   set QT_MULTIMEDIA_PREFERRED_PLUGINS=directshow";
    debugInfo << "   QtBarcodeScanner.exe";
    debugInfo << "";
    debugInfo << "3. Check Windows Media Feature Pack (N/KN editions):";
    debugInfo << "   Some Windows versions need Media Feature Pack installed";
    debugInfo << "";
    debugInfo << "4. Since OpenCV works, consider hybrid approach:";
    debugInfo << "   Use OpenCV for camera capture + Qt for UI";

    ui->resultsTextEdit->clear();
    ui->resultsTextEdit->append(debugInfo.join("\n"));
}

void MainWindow::stopCamera()
{
    // Clear camera overlay data
    currentCameraResults.clear();
    currentCameraFrame = QPixmap();

#ifdef ENABLE_OPENCV_CAMERA
    // Stop OpenCV camera if it's being used
    if (useOpenCVCamera && openCVCamera)
    {
        openCVCamera->stop();
        if (cameraLabel)
        {
            cameraLabel->hide();
            cameraLabel->clear();
        }
        useOpenCVCamera = false;
        ui->resultsTextEdit->append("OpenCV camera stopped.");
    }
#endif

    // Stop Qt6 camera if it's being used
    if (camera)
    {
        camera->stop();
        camera.reset();
        captureSession.reset();

        if (videoSurface)
        {
            videoSurface->deleteLater();
            videoSurface = nullptr;
        }

        if (videoWidget)
        {
            videoWidget->show(); // Make sure it's visible for next time
        }

        ui->resultsTextEdit->append("Qt6 camera stopped.");
    }

    ui->startCameraButton->setEnabled(true);
    ui->stopCameraButton->setEnabled(false);
}

void MainWindow::clearResults()
{
    ui->resultsTextEdit->clear();
}

void MainWindow::about()
{
    QMessageBox::about(this, "About Qt Barcode Scanner",
                       "Qt Barcode Scanner v1.0\n\n"
                       "A cross-platform barcode scanning application built with Qt 6 and Dynamsoft Barcode Reader.\n\n"
                       "Features:\n"
                       "- Image file barcode scanning\n"
                       "- Real-time camera barcode scanning\n"
                       "- Drag and drop support\n"
                       "- Configurable license and templates\n\n"
                       "© 2025 - Built with Qt and Dynamsoft SDK");
}

void MainWindow::setLicense()
{
    bool ok;
    QString newLicense = QInputDialog::getText(this, "Set License Key",
                                               "Enter your Dynamsoft Barcode Reader license key:", QLineEdit::Normal,
                                               licenseKey, &ok);

    if (ok && !newLicense.isEmpty())
    {
        licenseKey = newLicense;
        // Update the license in the worker
        QMetaObject::invokeMethod(barcodeWorker, "setLicense", Q_ARG(QString, licenseKey));
        QMessageBox::information(this, "License", "License key updated successfully.");
    }
}

void MainWindow::loadTemplate()
{
    QString fileName = QFileDialog::getOpenFileName(this,
                                                    "Load Barcode Template",
                                                    QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
                                                    "JSON Files (*.json);;All Files (*)");

    if (!fileName.isEmpty())
    {
        QFile file(fileName);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            templateContent = file.readAll();
            file.close();

            // Update the template in the worker
            QMetaObject::invokeMethod(barcodeWorker, "setTemplate", Q_ARG(QString, templateContent));
            QMessageBox::information(this, "Template",
                                     QString("Template loaded successfully from: %1").arg(QFileInfo(fileName).fileName()));
        }
        else
        {
            QMessageBox::warning(this, "Error", "Failed to read template file.");
        }
    }
}

void MainWindow::onBarcodeResults(const QList<BarcodeResult> &results)
{
    updateResultsDisplay(results);

    // If we're in image mode, update the image with detection boxes
    if (ui->tabWidget->currentIndex() == 0 && !currentPixmap.isNull())
    {
        updateImageDisplay(currentPixmap, results);
    }
    // If we're in camera mode, update camera display with overlay
    else if (ui->tabWidget->currentIndex() == 1)
    {
        currentCameraResults = results;
    }
}

void MainWindow::onImageTabSelected()
{
    // Clear camera results when switching away from camera
    currentCameraResults.clear();
    currentCameraFrame = QPixmap();

    // Stop camera if running
    if (camera && camera->isActive())
    {
        stopCamera();
    }
}

void MainWindow::onCameraTabSelected()
{
    // No specific action needed when switching to camera tab
}

void MainWindow::onLicenseStatusChanged(const QString &status, bool isValid)
{
    updateLicenseStatus(status, isValid);
}

void MainWindow::updateLicenseStatus(const QString &status, bool isValid)
{
    if (licenseStatusLabel)
    {
        licenseStatusLabel->setText(status);
        if (isValid)
        {
            licenseStatusLabel->setStyleSheet("QLabel { color: green; font-weight: bold; }");
        }
        else
        {
            licenseStatusLabel->setStyleSheet("QLabel { color: red; font-weight: bold; }");
        }
    }
}

void MainWindow::loadImageFile(const QString &filePath)
{
    // First check if file exists
    if (!QFile::exists(filePath))
    {
        QMessageBox::warning(this, "Error", QString("File does not exist: %1").arg(filePath));
        ui->resultsTextEdit->append(QString("Error: File not found - %1").arg(filePath));
        return;
    }

    // Check file size
    QFileInfo fileInfo(filePath);
    qint64 fileSize = fileInfo.size();
    ui->resultsTextEdit->append(QString("Loading image: %1 (Size: %2 bytes)").arg(fileInfo.fileName()).arg(fileSize));

    QImage image;
    QString loadMethod;

    // Try loading with Qt first
    QImageReader reader(filePath);
    if (reader.canRead())
    {
        QString format = reader.format();
        QSize imageSize = reader.size();
        ui->resultsTextEdit->append(QString("Qt detected format: %1, Size: %2x%3").arg(format).arg(imageSize.width()).arg(imageSize.height()));

        image = reader.read();
        loadMethod = QString("Qt (%1)").arg(format);
    }

#ifdef ENABLE_OPENCV_CAMERA
    // If Qt failed, try OpenCV as fallback
    if (image.isNull())
    {
        ui->resultsTextEdit->append("Qt failed to load image, trying OpenCV fallback...");
        try
        {
            cv::Mat cvImage = cv::imread(filePath.toStdString(), cv::IMREAD_COLOR);
            if (!cvImage.empty())
            {
                // Convert BGR to RGB
                cv::cvtColor(cvImage, cvImage, cv::COLOR_BGR2RGB);

                // Convert cv::Mat to QImage
                image = QImage(cvImage.data, cvImage.cols, cvImage.rows, cvImage.step, QImage::Format_RGB888);
                loadMethod = "OpenCV";
                ui->resultsTextEdit->append(QString("OpenCV loaded image successfully: %1x%2").arg(cvImage.cols).arg(cvImage.rows));
            }
            else
            {
                ui->resultsTextEdit->append("OpenCV also failed to load the image");
            }
        }
        catch (const std::exception &e)
        {
            ui->resultsTextEdit->append(QString("OpenCV exception: %1").arg(e.what()));
        }
    }
#endif

    // Final check if we have a valid image
    if (image.isNull())
    {
        QString errorMsg = QString("Failed to load image: %1").arg(fileInfo.fileName());
#ifdef ENABLE_OPENCV_CAMERA
        errorMsg += "\nBoth Qt and OpenCV failed to load this image.";
#else
        errorMsg += "\nQt failed to load this image. OpenCV fallback not available.";
#endif
        QMessageBox::warning(this, "Error", errorMsg);
        ui->resultsTextEdit->append("Error: All image loading methods failed");
        return;
    }

    // Convert to pixmap and display
    QPixmap pixmap = QPixmap::fromImage(image);
    if (!pixmap.isNull())
    {
        currentPixmap = pixmap;
        ui->imageLabel->setPixmap(pixmap.scaled(ui->imageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));

        // Process the image for barcodes
        qDebug() << "Invoking barcode processing for image:" << image.size() << "format:" << image.format();
        QMetaObject::invokeMethod(barcodeWorker, "processImage", Q_ARG(QImage, image));

        ui->resultsTextEdit->append(QString("Image loaded successfully via %1: %2").arg(loadMethod, fileInfo.fileName()));
        ui->resultsTextEdit->append("Processing image for barcodes...");
    }
    else
    {
        QMessageBox::warning(this, "Error", QString("Failed to create pixmap from image: %1").arg(fileInfo.fileName()));
        ui->resultsTextEdit->append("Error: Failed to create pixmap from loaded image");
    }
}

void MainWindow::updateImageDisplay(const QPixmap &pixmap, const QList<BarcodeResult> &results)
{
    QPixmap displayPixmap = pixmap;

    if (!results.isEmpty())
    {
        QPainter painter(&displayPixmap);
        painter.setPen(QPen(Qt::green, 3));
        painter.setFont(QFont("Arial", 12, QFont::Bold));

        for (const auto &result : results)
        {
            if (result.points.size() >= 4)
            {
                // Draw bounding box
                QPolygon polygon;
                for (const auto &point : result.points)
                {
                    polygon << point;
                }
                painter.drawPolygon(polygon);

                // Draw text
                if (!result.points.isEmpty())
                {
                    painter.fillRect(result.points[0].x(), result.points[0].y() - 20,
                                     painter.fontMetrics().horizontalAdvance(result.text) + 10, 20,
                                     QColor(0, 255, 0, 180));
                    painter.setPen(Qt::black);
                    painter.drawText(result.points[0].x() + 5, result.points[0].y() - 5, result.text);
                    painter.setPen(QPen(Qt::green, 3));
                }
            }
        }
    }

    ui->imageLabel->setPixmap(displayPixmap.scaled(ui->imageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void MainWindow::updateResultsDisplay(const QList<BarcodeResult> &results)
{
    if (results.isEmpty())
    {
        ui->resultsTextEdit->append("No barcodes detected.");
        return;
    }

    ui->resultsTextEdit->append(QString("--- Detected %1 barcode(s) ---").arg(results.size()));

    for (int i = 0; i < results.size(); ++i)
    {
        const auto &result = results[i];
        ui->resultsTextEdit->append(QString("Barcode %1:").arg(i + 1));
        ui->resultsTextEdit->append(QString("  Format: %1").arg(result.format));
        ui->resultsTextEdit->append(QString("  Text: %1").arg(result.text));
        if (!result.points.isEmpty())
        {
            QString pointsStr = "  Points: ";
            for (const auto &point : result.points)
            {
                pointsStr += QString("(%1,%2) ").arg(point.x()).arg(point.y());
            }
            ui->resultsTextEdit->append(pointsStr);
        }
        ui->resultsTextEdit->append("");
    }

    // Auto-scroll to bottom
    QTextCursor cursor = ui->resultsTextEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    ui->resultsTextEdit->setTextCursor(cursor);
}
