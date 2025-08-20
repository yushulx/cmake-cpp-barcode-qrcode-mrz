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

// Qt6 Camera includes
#include <QtMultimedia/QCamera>
#include <QtMultimedia/QMediaCaptureSession>
#include <QtMultimediaWidgets/QVideoWidget>
#include <QtMultimedia/QMediaDevices>

#include <opencv2/opencv.hpp>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), camera(nullptr), captureSession(nullptr), videoWidget(nullptr), workerThread(nullptr), barcodeWorker(nullptr), licenseKey("LICENSE-KEY"), lastImageDirectory(QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)),
      openCVCamera(nullptr), cameraLabel(nullptr), useOpenCVCamera(false), resizeTimer(nullptr), cameraUpdatesPaused(false)
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
            
            // Get the current label size (use the actual available space)
            QSize labelSize = cameraLabel->size();
            
            // Ensure we have a reasonable minimum size and the label has been properly sized
            if (labelSize.width() > 10 && labelSize.height() > 10) {
                QPixmap displayPixmap = pixmap;
                
                // Check if we have current barcode results to overlay
                if (!currentCameraResults.isEmpty()) {
                    // Create overlay on the current frame
                    QPainter painter(&displayPixmap);
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
                }

                // Scale the pixmap to fit within the label while maintaining aspect ratio
                QPixmap scaledPixmap = displayPixmap.scaled(
                    labelSize, 
                    Qt::KeepAspectRatio, 
                    Qt::SmoothTransformation
                );
                cameraLabel->setPixmap(scaledPixmap);
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
                }
            }
        }, Qt::QueuedConnection); });

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

    // List supported image formats
    QList<QByteArray> supportedFormats = QImageReader::supportedImageFormats();
    QStringList formatList;
    for (const QByteArray &format : supportedFormats)
    {
        formatList << QString(format);
    }

    workerThread->start();
}

MainWindow::~MainWindow()
{
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
    if (openCVCamera)
    {
        openCVCamera->stop();
    }
    event->accept();
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    // Always call parent first
    QMainWindow::resizeEvent(event);

    // Safety check - don't process resize events during destruction
    if (!this || !ui || cameraUpdatesPaused)
        return;

    // Handle image resizing in image tab
    if (ui->tabWidget->currentIndex() == 0 && !currentPixmap.isNull())
    {
        // Use a timer to defer the image update to avoid excessive updates during rapid resizing
        static QTimer *imageResizeTimer = nullptr;
        if (!imageResizeTimer)
        {
            imageResizeTimer = new QTimer(this);
            imageResizeTimer->setSingleShot(true);
            imageResizeTimer->setInterval(50); // 50ms delay for smooth resizing
            connect(imageResizeTimer, &QTimer::timeout, this, [this]()
                    {
                // Re-scale the current image to fit the new label size
                if (!currentPixmap.isNull() && ui->imageLabel)
                {
                    if (!currentImageResults.isEmpty())
                    {
                        // If we have barcode results, redraw with overlay
                        updateImageDisplay(currentPixmap, currentImageResults);
                    }
                    else
                    {
                        // Just resize the image without overlay
                        updateImageDisplay(currentPixmap);
                    }
                } });
        }
        imageResizeTimer->start();
    }

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
            }
        }
    }
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);

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
            cameraUpdatesPaused = false; });

        // Window state changed - camera updates paused temporarily
    }
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

        // First try Qt6 Multimedia Camera
        if (!camera)
        {
            QList<QCameraDevice> cameras = QMediaDevices::videoInputs();
            if (!cameras.isEmpty())
            {
                ui->resultsTextEdit->append(QString("Found %1 Qt camera(s)").arg(cameras.size()));
                
                // Create camera and capture session
                camera = std::make_unique<QCamera>(cameras.first());
                captureSession = std::make_unique<QMediaCaptureSession>();
                
                captureSession->setCamera(camera.get());
                captureSession->setVideoOutput(videoWidget.get());
                
                // Start camera
                camera->start();
                
                // Show Qt video widget, hide OpenCV label
                videoWidget->show();
                if (cameraLabel) cameraLabel->hide();
                useOpenCVCamera = false;
                
                ui->startCameraButton->setEnabled(false);
                ui->stopCameraButton->setEnabled(true);
                
                ui->resultsTextEdit->append("Qt6 camera started successfully!");
                return;
            }
            else
            {
                ui->resultsTextEdit->append("No Qt cameras detected, trying OpenCV...");
            }
        }

        // Fall back to OpenCV camera
        if (tryStartOpenCVCamera())
        {
            return; // Success with OpenCV
        }

        // If we get here, both Qt and OpenCV failed
        // Run simple camera diagnostics
        ui->resultsTextEdit->append("\n=== Camera Diagnostics ===");
        ui->resultsTextEdit->append("No cameras found with Qt6 Multimedia or OpenCV.");
        ui->resultsTextEdit->append("Check Windows Camera privacy settings if camera fails.");
        ui->resultsTextEdit->append("Ensure camera is not in use by other applications.");

        QMessageBox::information(this, "Camera Info",
                                 "No camera devices found.\n\n"
                                 "You can still use the file scanning feature.");
    }
    catch (const std::exception &e)
    {
        QMessageBox::warning(this, "Camera Error",
                             QString("Failed to start camera: %1").arg(e.what()));
    }
}

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
        // Show OpenCV camera label, hide Qt video widget
        cameraLabel->show();
        if (videoWidget) videoWidget->hide();
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

void MainWindow::stopCamera()
{
    // Clear camera overlay data
    currentCameraResults.clear();
    currentCameraFrame = QPixmap();

    // Stop Qt6 camera if it's being used
    if (camera && !useOpenCVCamera)
    {
        camera->stop();
        camera.reset();
        captureSession.reset();
        if (videoWidget)
        {
            videoWidget->hide();
        }
        ui->resultsTextEdit->append("Qt6 camera stopped.");
    }

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

    ui->startCameraButton->setEnabled(true);
    ui->stopCameraButton->setEnabled(false);
}

void MainWindow::clearResults()
{
    ui->resultsTextEdit->clear();

    // Clear stored results
    currentImageResults.clear();
    currentCameraResults.clear();

    // If we're in image mode and have an image loaded, redisplay without overlay
    if (ui->tabWidget->currentIndex() == 0 && !currentPixmap.isNull())
    {
        updateImageDisplay(currentPixmap);
    }
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
        currentImageResults = results; // Store results for resize handling
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
    if (openCVCamera && useOpenCVCamera)
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

    // Final check if we have a valid image
    if (image.isNull())
    {
        QString errorMsg = QString("Failed to load image: %1").arg(fileInfo.fileName());
        errorMsg += "\nBoth Qt and OpenCV failed to load this image.";
        QMessageBox::warning(this, "Error", errorMsg);
        ui->resultsTextEdit->append("Error: All image loading methods failed");
        return;
    }

    // Convert to pixmap and display
    QPixmap pixmap = QPixmap::fromImage(image);
    if (!pixmap.isNull())
    {
        currentPixmap = pixmap;
        currentImageResults.clear(); // Clear previous results when loading new image
        ui->imageLabel->setPixmap(pixmap.scaled(ui->imageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));

        // Process the image for barcodes
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
