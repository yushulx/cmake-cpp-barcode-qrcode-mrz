#include "barcodeworker.h"
#include <QLoggingCategory>

BarcodeWorker::BarcodeWorker(QObject *parent)
    : QObject(parent), m_router(nullptr), m_licenseKey("DLS2eyJoYW5kc2hha2VDb2RlIjoiMjAwMDAxLTE2NDk4Mjk3OTI2MzUiLCJvcmdhbml6YXRpb25JRCI6IjIwMDAwMSIsInNlc3Npb25QYXNzd29yZCI6IndTcGR6Vm05WDJrcEQ5YUoifQ=="), m_initialized(false)
{
    qRegisterMetaType<BarcodeResult>("BarcodeResult");
    qRegisterMetaType<QList<BarcodeResult>>("QList<BarcodeResult>");
}

BarcodeWorker::~BarcodeWorker()
{
    if (m_router)
    {
        delete m_router;
        m_router = nullptr;
    }
}

void BarcodeWorker::initialize()
{
    try
    {
        // Initialize license
        char errorMsgBuffer[512];
        int ret = CLicenseManager::InitLicense(m_licenseKey.toUtf8().constData(), errorMsgBuffer, 512);
        if (ret != EC_OK)
        {
            QString errorMsg = QString::fromUtf8(errorMsgBuffer);
            qWarning() << "License initialization failed:" << errorMsg;
            qWarning() << "License error code:" << ret;
            emit licenseStatusChanged(QString("License: Failed (%1)").arg(errorMsg), false);
        }
        else
        {
            emit licenseStatusChanged("License: Valid", true);
        }

        // Create capture vision router
        m_router = new CCaptureVisionRouter();
        if (!m_router)
        {
            qWarning() << "Failed to create CCaptureVisionRouter";
            return;
        }

        // Add this as result receiver
        m_router->AddResultReceiver(this);

        m_initialized = true;
    }
    catch (const std::exception &e)
    {
        qWarning() << "Failed to initialize BarcodeWorker:" << e.what();
        m_initialized = false;
    }
}

void BarcodeWorker::processImage(const QImage &image)
{
    if (!m_initialized || !m_router || image.isNull())
    {
        if (!m_initialized)
        {
            initialize();
            if (!m_initialized)
            {
                emit resultsReady(QList<BarcodeResult>());
                return;
            }
        }

        if (image.isNull())
        {
            emit resultsReady(QList<BarcodeResult>());
        }
    }

    try
    {
        // Convert QImage to format suitable for Dynamsoft
        QImage rgbImage = image.convertToFormat(QImage::Format_RGB888);

        // Create CImageData using constructor
        CImageData imageData(
            rgbImage.sizeInBytes(),  // bytesLength
            rgbImage.bits(),         // bytes
            rgbImage.width(),        // width
            rgbImage.height(),       // height
            rgbImage.bytesPerLine(), // stride
            IPF_RGB_888              // format
        );

        // Process the image using the barcode reading preset template
        CCapturedResult *result = m_router->Capture(&imageData, CPresetTemplate::PT_READ_BARCODES);

        if (result)
        {
            if (result->GetErrorCode() != EC_OK)
            {
                qWarning() << "Capture error:" << result->GetErrorString();
                emit resultsReady(QList<BarcodeResult>());
            }
            else
            {
                // Get barcode results
                CDecodedBarcodesResult *barcodeResult = result->GetDecodedBarcodesResult();

                if (barcodeResult)
                {
                    // Process results directly
                    QList<BarcodeResult> results = convertResults(barcodeResult);
                    emit resultsReady(results);
                }
                else
                {
                    emit resultsReady(QList<BarcodeResult>());
                }
            }
            result->Release();
        }
        else
        {
            qWarning() << "Capture returned null result";
            emit resultsReady(QList<BarcodeResult>());
        }
    }
    catch (const std::exception &e)
    {
        qWarning() << "Error processing image:" << e.what();
        emit resultsReady(QList<BarcodeResult>());
    }
}

void BarcodeWorker::processFrame(const QImage &frame)
{
    // Use the same processing as images for now
    processImage(frame);
}

void BarcodeWorker::setLicense(const QString &license)
{
    m_licenseKey = license;

    if (m_initialized)
    {
        // Re-initialize with new license
        char errorMsgBuffer[512];
        int ret = CLicenseManager::InitLicense(m_licenseKey.toUtf8().constData(), errorMsgBuffer, 512);
        if (ret != EC_OK)
        {
            QString errorMsg = QString::fromUtf8(errorMsgBuffer);
            qWarning() << "License update failed:" << errorMsg;
            emit licenseStatusChanged(QString("License: Failed (%1)").arg(errorMsg), false);
        }
        else
        {
            emit licenseStatusChanged("License: Valid", true);
        }
    }
    else
    {
        emit licenseStatusChanged("License: Updated (Pending Initialization)", false);
    }
}

void BarcodeWorker::setTemplate(const QString &templateContent)
{
    m_templateContent = templateContent;

    if (m_initialized && m_router && !templateContent.isEmpty())
    {
        try
        {
            char errorMsgBuffer[512];
            int ret = m_router->InitSettings(templateContent.toUtf8().constData(), errorMsgBuffer, 512);
            if (ret != EC_OK)
            {
                qWarning() << "Template initialization failed:" << errorMsgBuffer;
            }
            else
            {
                // Template loaded successfully
            }
        }
        catch (const std::exception &e)
        {
            qWarning() << "Error setting template:" << e.what();
        }
    }
}

void BarcodeWorker::OnDecodedBarcodesReceived(CDecodedBarcodesResult *pResult)
{
    if (!pResult)
    {
        return;
    }

    if (pResult->GetErrorCode() != EC_OK)
    {
        return;
    }

    QList<BarcodeResult> results = convertResults(pResult);
    emit resultsReady(results);
}

QList<BarcodeResult> BarcodeWorker::convertResults(CDecodedBarcodesResult *pResult)
{
    QList<BarcodeResult> results;

    if (!pResult || pResult->GetErrorCode() != EC_OK)
    {
        return results;
    }

    int count = pResult->GetItemsCount();
    for (int i = 0; i < count; i++)
    {
        const CBarcodeResultItem *item = pResult->GetItem(i);
        if (!item)
        {
            continue;
        }

        BarcodeResult result;
        result.format = QString::fromUtf8(item->GetFormatString());
        result.text = QString::fromUtf8(item->GetText());

        // Get localization points - use the same method as command-line example
        CPoint *points = item->GetLocation().points;
        result.points.clear();
        for (int j = 0; j < 4; j++)
        {
            result.points.append(QPoint(points[j][0], points[j][1]));
        }

        results.append(result);
    }

    return results;
}
