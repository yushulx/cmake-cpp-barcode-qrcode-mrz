#include "mainwindow.h"
#include "./ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setAcceptDrops(true);

    worker = NULL;
    thread = NULL;
    camera = NULL;

    // Dynamsoft Label Recognizer
    recognizer = DLR_CreateInstance();
    char errorMsgBuffer[256];
    DLR_AppendSettingsFromFile(recognizer, "MRZ.json", errorMsgBuffer, 256);

    // Open a file.
    connect(ui->actionOpen_File, SIGNAL(triggered()), this, SLOT(openFile()));

    // Open a folder.
    connect(ui->actionOpen_Folder, SIGNAL(triggered()), this, SLOT(openFolder()));

    // List widget event.
    connect(ui->listWidget, SIGNAL(itemClicked(QListWidgetItem *)), this, SLOT(listWidgetClicked(QListWidgetItem *)));

    // Export template.
    connect(ui->actionExport_template, SIGNAL(triggered()), this, SLOT(exportTemplate()));

    // About dialog.
    connect(ui->actionAbout, SIGNAL(triggered()), this, SLOT(about()));

    // Set license.
    connect(ui->actionEnter_License_Key, SIGNAL(triggered()), this, SLOT(setLicense()));

    // Camera button
    connect(ui->pushButton_open, SIGNAL(clicked()), this, SLOT(startCamera()));
    connect(ui->pushButton_stop, SIGNAL(clicked()), this, SLOT(stopCamera()));

    // Cameras
    QList<QCameraInfo> cameras = QCameraInfo::availableCameras();
    if (cameras.size() > 0)
    {
        for (int i = 0; i < cameras.count(); ++i)
        {
            QCameraInfo cameraInfo = cameras.at(i);
            qDebug() << cameraInfo.deviceName();
            qDebug() << cameraInfo.description();
            camera = new QCamera(cameraInfo);
            surface = new MyVideoSurface(this, ui, recognizer, camera);
            camera->setViewfinder(surface);
            break;
        }
    }
    else
    {
        ui->pushButton_open->setEnabled(false);
        ui->pushButton_stop->setEnabled(false);
    }
}

MainWindow::~MainWindow()
{
    camera->stop();
    delete ui;
    DLR_DestroyInstance(recognizer);
    delete camera;
    delete surface;
}

void MainWindow::loadFile(QString fileName)
{
    stopCamera();

    // Add to list
    ui->listWidget->addItem(fileName);
    ui->statusbar->showMessage(fileName);

    // Load the image file to QImage
    QImage image(fileName);
    showImage(image, fileName);
}

void MainWindow::openFile()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"), "", tr("Passport images (*)"));
    if (!fileName.isEmpty())
    {
        loadFile(fileName);
    }
}

void MainWindow::openFolder()
{
    QString folderName = QFileDialog::getExistingDirectory(this, tr("Open Folder"), "", QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (!folderName.isEmpty())
    {
        // Get all files in the folder
        QStringList fileNames = QDir(folderName).entryList(QDir::Files | QDir::NoDotAndDotDot);
        // Add to list
        for (int i = 0; i < fileNames.size(); i++)
        {
            ui->listWidget->addItem(QDir::cleanPath(folderName + QDir::separator() + fileNames.at(i)));
        }

        ui->statusbar->showMessage(folderName);
    }
}

void MainWindow::listWidgetClicked(QListWidgetItem *item)
{
    ui->statusbar->showMessage(QString(item->text()));

    // Load the image file to QImage
    QImage image(item->text());
    showImage(image, item->text());
}

void MainWindow::showImage(const QImage &image, QString fileName)
{
    ui->textEdit_results->setText("");
    if (!image.isNull())
    {
        QPixmap pm = QPixmap::fromImage(image);
        QPainter painter(&pm);
        painter.setPen(Qt::red);

        /************************
         * MRZ detection.
         ************************/

        QDateTime start = QDateTime::currentDateTime();
        int errorCode = DLR_RecognizeByFile(recognizer, fileName.toStdString().c_str(), "locr");
        QDateTime end = QDateTime::currentDateTime();
        DLR_ResultArray *handler = NULL;
        DLR_GetAllResults(recognizer, &handler);

        if (handler == NULL || handler->resultsCount == 0)
        {
            QString message = "No passport info found. Elapsed time: " + QString::number(start.msecsTo(end)) + "ms\n";
            ui->textEdit_results->setText(message);
            DLR_FreeResults(&handler);
            ui->label->setPixmap(pm.scaled(ui->label->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            return;
        }

        QString out = "Elapsed time: " + QString::number(start.msecsTo(end)) + "ms\n\n";
        DLR_Result **results = handler->results;
        for (int ri = 0; ri < handler->resultsCount; ++ri)
        {
            DLR_Result *result = handler->results[ri];
            int lCount = result->lineResultsCount;
            for (int li = 0; li < lCount; ++li)
            {
                DM_Point *points = result->lineResults[li]->location.points;
                int x1 = points[0].x, y1 = points[0].y;
                int x2 = points[1].x, y2 = points[1].y;
                int x3 = points[2].x, y3 = points[2].y;
                int x4 = points[3].x, y4 = points[3].y;

                painter.drawLine(x1, y1, x2, y2);
                painter.drawLine(x2, y2, x3, y3);
                painter.drawLine(x3, y3, x4, y4);
                painter.drawLine(x4, y4, x1, y1);
            }

            if (lCount < 2)
            {
                continue;
            }
            string line1 = result->lineResults[0]->text;
            string line2 = result->lineResults[1]->text;
            if (line1.length() != 44 || line2.length() != 44)
            {
                continue;
            }
            if (line1[0] != 'P')
                continue;
            else
            {
                // https://en.wikipedia.org/wiki/Machine-readable_passport
                // Type
                string tmp = "Type: ";
                tmp.insert(tmp.length(), 1, line1[0]);
                out += QString::fromStdString(tmp) + "\n";

                // Issuing country
                tmp = "Issuing country: ";
                line1.substr(2, 5);
                tmp += line1.substr(2, 3);
                out += QString::fromStdString(tmp) + "\n";

                // Surname
                int index = 5;
                tmp = "Surname: ";
                for (; index < 44; index++)
                {
                    if (line1[index] != '<')
                    {
                        tmp.insert(tmp.length(), 1, line1[index]);
                    }
                    else
                    {
                        break;
                    }
                }
                out += QString::fromStdString(tmp) + "\n";

                // Given names
                tmp = "Given Names: ";
                index += 2;
                for (; index < 44; index++)
                {
                    if (line1[index] != '<')
                    {
                        tmp.insert(tmp.length(), 1, line1[index]);
                    }
                    else
                    {
                        tmp.insert(tmp.length(), 1, ' ');
                    }
                }
                out += QString::fromStdString(tmp) + "\n";

                // Passport number
                tmp = "Passport number: ";
                index = 0;
                for (; index < 9; index++)
                {
                    if (line2[index] != '<')
                    {
                        tmp.insert(tmp.length(), 1, line2[index]);
                    }
                    else
                    {
                        break;
                    }
                }
                out += QString::fromStdString(tmp) + "\n";

                // Nationality
                tmp = "Nationality: ";
                tmp += line2.substr(10, 3);
                out += QString::fromStdString(tmp) + "\n";

                // Date of birth
                tmp = line2.substr(13, 6);
                tmp.insert(2, "/");
                tmp.insert(5, "/");
                tmp = "Date of birth (YYMMDD): " + tmp;
                out += QString::fromStdString(tmp) + "\n";

                // Sex
                tmp = "Sex: ";
                tmp.insert(tmp.length(), 1, line2[20]);
                out += QString::fromStdString(tmp) + "\n";

                // Expiration date of passport
                tmp = line2.substr(21, 6);
                tmp.insert(2, "/");
                tmp.insert(5, "/");
                tmp = "Expiration date of passport (YYMMDD): " + tmp;
                out += QString::fromStdString(tmp) + "\n";

                // Personal number
                if (line2[28] != '<')
                {
                    tmp = "Personal number: ";
                    for (index = 28; index < 42; index++)
                    {
                        if (line2[index] != '<')
                        {
                            tmp.insert(tmp.length(), 1, line2[index]);
                        }
                        else
                        {
                            break;
                        }
                    }
                    out += QString::fromStdString(tmp) + "\n";
                }
            }
        }

        DLR_FreeResults(&handler);

        painter.end();
        ui->label->setPixmap(pm.scaled(ui->label->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));

        ui->textEdit_results->setText(out);
    }
}

void MainWindow::exportTemplate()
{
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save File"), "", tr("MRZ Template (*.json)"));
    if (recognizer)
    {
        DLR_OutputSettingsToFile(recognizer, fileName.toStdString().c_str(), "locr");
    }
}

void MainWindow::about()
{
    showMessageBox(tr("About"), tr("<a href='https://www.dynamsoft.com/label-recognition/overview/'>Dynamsoft Label Recognizer v2.0</a>"));
}

void MainWindow::showMessageBox(QString title, QString content)
{
    QMessageBox::information(this, title, content);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    QMessageBox::StandardButton reply = QMessageBox::question(this, tr("Exit"), tr("Are you sure to quit?"), QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply == QMessageBox::Yes)
    {
        event->accept();
    }
    else
    {
        event->ignore();
    }
}

void MainWindow::setLicense()
{
    QString licenseKey = QInputDialog::getText(this, tr("Enter License Key"), tr("License Key:"));
    char errorMsgBuffer[256];
    int ret = DLR_InitLicense(licenseKey.toStdString().c_str(), errorMsgBuffer, 256);
    if (ret != 0)
    {
        showMessageBox(tr("License Key"), tr("License key is invalid."));
    }
    else
    {
        showMessageBox(tr("License Key"), tr("Dynamsoft Label recognizer is activated successfully!"));
    }
}

void MainWindow::startCamera()
{
    surface->reset();

    thread = new QThread();
    worker = new Work(ui, recognizer, surface); // Do not set a parent. The object cannot be moved if it has a parent.
    worker->moveToThread(thread);
    connect(thread, SIGNAL(finished()), worker, SLOT(deleteLater()));
    connect(thread, SIGNAL(started()), worker, SLOT(detectMRZ()));
    thread->start();

    surface->setWorker(worker);

    camera->start();
}

void MainWindow::stopCamera()
{
    if (worker)
        worker->stop();
    if (camera && camera->state() == QCamera::ActiveState)
    {
        qDebug() << camera->state() << endl;
        camera->stop();
    }
    if (thread)
    {
        thread->quit();
        thread->wait();
    }

    worker = NULL;
    thread = NULL;
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const QMimeData *mimeData = event->mimeData();
    foreach (QUrl url, event->mimeData()->urls())
    {
        QString fileName = url.toLocalFile();
        loadFile(fileName);
    }
    event->acceptProposedAction();
}