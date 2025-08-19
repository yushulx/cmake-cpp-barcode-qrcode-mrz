#ifndef BARCODEWORKER_H
#define BARCODEWORKER_H

#include <QtCore/QObject>
#include <QtCore/QList>
#include <QtCore/QPoint>
#include <QtGui/QImage>

#include "DynamsoftCaptureVisionRouter.h"
#include "DynamsoftUtility.h"

using namespace dynamsoft::license;
using namespace dynamsoft::cvr;
using namespace dynamsoft::dbr;
using namespace dynamsoft::utility;
using namespace dynamsoft::basic_structures;

struct BarcodeResult
{
    QString format;
    QString text;
    QList<QPoint> points;

    BarcodeResult() = default;
    BarcodeResult(const QString &f, const QString &t, const QList<QPoint> &p)
        : format(f), text(t), points(p) {}
};

Q_DECLARE_METATYPE(BarcodeResult)
Q_DECLARE_METATYPE(QList<BarcodeResult>)

class BarcodeWorker : public QObject, public CCapturedResultReceiver
{
    Q_OBJECT

public:
    explicit BarcodeWorker(QObject *parent = nullptr);
    ~BarcodeWorker();

    // CCapturedResultReceiver interface
    virtual void OnDecodedBarcodesReceived(CDecodedBarcodesResult *pResult) override;

public slots:
    void initialize();
    void processImage(const QImage &image);
    void processFrame(const QImage &frame);
    void setLicense(const QString &license);
    void setTemplate(const QString &templateContent);

signals:
    void resultsReady(const QList<BarcodeResult> &results);
    void licenseStatusChanged(const QString &status, bool isValid);

private:
    CCaptureVisionRouter *m_router;
    QString m_licenseKey;
    QString m_templateContent;
    bool m_initialized;

    void initializeRouter();
    QList<BarcodeResult> convertResults(CDecodedBarcodesResult *pResult);
};

#endif // BARCODEWORKER_H
