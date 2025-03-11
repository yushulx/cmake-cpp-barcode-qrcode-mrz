#include <iostream>
#include <deque>
#include <vector>
#include <mutex>
#include <string>
#include "DynamsoftCaptureVisionRouter.h"
#include "DynamsoftUtility.h"
#include "Camera.h"
#include "CameraPreview.h"

using namespace std;
using namespace dynamsoft::license;
using namespace dynamsoft::dlr;
using namespace dynamsoft::cvr;
using namespace dynamsoft::dbr;
using namespace dynamsoft::utility;
using namespace dynamsoft::basic_structures;
using namespace dynamsoft::dcp;

class MRZResult
{
public:
    string docId;
    string docType;
    string nationality;
    string issuer;
    string dateOfBirth;
    string dateOfExpiry;
    string gender;
    string surname;
    string givenname;

    vector<string> rawText;

    MRZResult FromParsedResultItem(const CParsedResultItem *item)
    {
        docType = item->GetCodeType();

        if (docType == "MRTD_TD3_PASSPORT")
        {
            if (item->GetFieldValidationStatus("passportNumber") != VS_FAILED && item->GetFieldValue("passportNumber") != NULL)
            {
                docId = item->GetFieldValue("passportNumber");
            }
        }
        else if (item->GetFieldValidationStatus("documentNumber") != VS_FAILED && item->GetFieldValue("documentNumber") != NULL)
        {
            docId = item->GetFieldValue("documentNumber");
        }

        string line;
        if (docType == "MRTD_TD1_ID")
        {
            if (item->GetFieldValue("line1") != NULL)
            {
                line = item->GetFieldValue("line1");
                if (item->GetFieldValidationStatus("line1") == VS_FAILED)
                {
                    line += ", Validation Failed";
                }
                rawText.push_back(line);
            }

            if (item->GetFieldValue("line2") != NULL)
            {
                line = item->GetFieldValue("line2");
                if (item->GetFieldValidationStatus("line2") == VS_FAILED)
                {
                    line += ", Validation Failed";
                }
                rawText.push_back(line);
            }

            if (item->GetFieldValue("line3") != NULL)
            {
                line = item->GetFieldValue("line3");
                if (item->GetFieldValidationStatus("line3") == VS_FAILED)
                {
                    line += ", Validation Failed";
                }
                rawText.push_back(line);
            }
        }
        else
        {
            if (item->GetFieldValue("line1") != NULL)
            {
                line = item->GetFieldValue("line1");
                if (item->GetFieldValidationStatus("line1") == VS_FAILED)
                {
                    line += ", Validation Failed";
                }
                rawText.push_back(line);
            }

            if (item->GetFieldValue("line2") != NULL)
            {
                line = item->GetFieldValue("line2");
                if (item->GetFieldValidationStatus("line2") == VS_FAILED)
                {
                    line += ", Validation Failed";
                }
                rawText.push_back(line);
            }
        }

        if (item->GetFieldValidationStatus("nationality") != VS_FAILED && item->GetFieldValue("nationality") != NULL)
        {
            nationality = item->GetFieldValue("nationality");
        }
        if (item->GetFieldValidationStatus("issuingState") != VS_FAILED && item->GetFieldValue("issuingState") != NULL)
        {
            issuer = item->GetFieldValue("issuingState");
        }
        if (item->GetFieldValidationStatus("dateOfBirth") != VS_FAILED && item->GetFieldValue("dateOfBirth") != NULL)
        {
            dateOfBirth = item->GetFieldValue("dateOfBirth");
        }
        if (item->GetFieldValidationStatus("dateOfExpiry") != VS_FAILED && item->GetFieldValue("dateOfExpiry") != NULL)
        {
            dateOfExpiry = item->GetFieldValue("dateOfExpiry");
        }
        if (item->GetFieldValidationStatus("sex") != VS_FAILED && item->GetFieldValue("sex") != NULL)
        {
            gender = item->GetFieldValue("sex");
        }
        if (item->GetFieldValidationStatus("primaryIdentifier") != VS_FAILED && item->GetFieldValue("primaryIdentifier") != NULL)
        {
            surname = item->GetFieldValue("primaryIdentifier");
        }
        if (item->GetFieldValidationStatus("secondaryIdentifier") != VS_FAILED && item->GetFieldValue("secondaryIdentifier") != NULL)
        {
            givenname = item->GetFieldValue("secondaryIdentifier");
        }

        return *this;
    }

    string ToString()
    {
        string msg = "Raw Text:\n";
        for (size_t idx = 0; idx < rawText.size(); ++idx)
        {
            msg += "\tLine " + to_string(idx + 1) + ": " + rawText[idx] + "\n";
        }
        msg += "Parsed Information:\n";
        msg += "\tDocument Type: " + docType + "\n";
        msg += "\tDocument ID: " + docId + "\n";
        msg += "\tSurname: " + surname + "\n";
        msg += "\tGiven Name: " + givenname + "\n";
        msg += "\tNationality: " + nationality + "\n";
        msg += "\tIssuing Country or Organization: " + issuer + "\n";
        msg += "\tGender: " + gender + "\n";
        msg += "\tDate of Birth(YYMMDD): " + dateOfBirth + "\n";
        msg += "\tExpiration Date(YYMMDD): " + dateOfExpiry + "\n";

        return msg;
    }
};

class Point
{

public:
    int x;
    int y;
    Point(int x, int y) : x(x), y(y) {}
};

struct TextResult
{
    int id;
    MRZResult info;
    std::vector<Point> textLinePoints;
};

// Shared data: Barcode results queue
const size_t MAX_QUEUE_SIZE = 4; // Maximum size of the queue
std::vector<TextResult> textResults;
std::mutex textResultsMutex;

class MyCapturedResultReceiver : public CCapturedResultReceiver
{
public:
    virtual void OnRecognizedTextLinesReceived(CRecognizedTextLinesResult *pResult) override
    {
        std::lock_guard<std::mutex> lock(textResultsMutex);
        textResults.clear();

        const CImageTag *tag = pResult->GetOriginalImageTag();

        if (tag == nullptr)
        {
            return;
        }

        if (pResult->GetErrorCode() != EC_OK)
        {
            cout << "Error: " << pResult->GetErrorString() << endl;
        }
        else
        {
            int lCount = pResult->GetItemsCount();
            for (int li = 0; li < lCount; ++li)
            {
                TextResult result;

                const CTextLineResultItem *textLine = pResult->GetItem(li);
                CPoint *points = textLine->GetLocation().points;
                result.textLinePoints.push_back(Point(points[0][0], points[0][1]));
                result.textLinePoints.push_back(Point(points[1][0], points[1][1]));
                result.textLinePoints.push_back(Point(points[2][0], points[2][1]));
                result.textLinePoints.push_back(Point(points[3][0], points[3][1]));

                result.id = tag->GetImageId();
                textResults.push_back(result);
            }
        }
    }

    virtual void OnParsedResultsReceived(CParsedResult *pResult)
    {
        if (pResult == nullptr)
        {
            return;
        }

        const CImageTag *tag = pResult->GetOriginalImageTag();

        if (tag == nullptr)
        {
            return;
        }

        if (pResult->GetErrorCode() != EC_OK)
        {
            cout << "Error: " << pResult->GetErrorString() << endl;
        }
        else
        {
            int lCount = pResult->GetItemsCount();
            for (int i = 0; i < lCount; i++)
            {
                const CParsedResultItem *item = pResult->GetItem(i);

                MRZResult result;
                result.FromParsedResultItem(item);
                cout << result.ToString() << endl;

                if (textResults[0].id == tag->GetImageId())
                {
                    std::lock_guard<std::mutex> lock(textResultsMutex);
                    textResults[0].info = result;
                }
            }
        }

        pResult->Release();
    }
};

class MyVideoFetcher : public CImageSourceAdapter
{
public:
    MyVideoFetcher() {}
    ~MyVideoFetcher() {}

    bool HasNextImageToFetch() const override
    {
        return true;
    }

    void MyAddImageToBuffer(const CImageData *img, bool bClone = true)
    {
        AddImageToBuffer(img, bClone);
    }
};

int main()
{
    int iRet = -1;
    char szErrorMsg[256];
    // Initialize license.
    // Request a trial from https://www.dynamsoft.com/customer/license/trialLicense/?product=dcv&package=cross-platform
    iRet = CLicenseManager::InitLicense("LICENSE-KEY", szErrorMsg, 256);
    if (iRet != EC_OK)
    {
        std::cout << szErrorMsg << std::endl;
    }

    int errorCode = 0;
    char errorMsg[512] = {0};
    CCaptureVisionRouter *cvr = new CCaptureVisionRouter;

    MyVideoFetcher *fetcher = new MyVideoFetcher();
    fetcher->SetMaxImageCount(4);
    fetcher->SetBufferOverflowProtectionMode(BOPM_UPDATE);
    fetcher->SetColourChannelUsageType(CCUT_AUTO);
    cvr->SetInput(fetcher);

    CCapturedResultReceiver *capturedReceiver = new MyCapturedResultReceiver;
    cvr->AddResultReceiver(capturedReceiver);

    errorCode = cvr->StartCapturing("ReadPassportAndId", false, errorMsg, 512);
    if (errorCode != EC_OK)
    {
        std::cout << "error:" << errorMsg << std::endl;
        return -1;
    }

    Camera camera;
    std::cout << "Capturing frames...\n";

    if (camera.Open(0))
    {
        // Create a window
        CameraWindow window(camera.frameWidth, camera.frameHeight, "Camera Stream");
        if (!window.Create())
        {
            std::cerr << "Failed to create window." << std::endl;
            return -1;
        }
        window.Show();

        CameraWindow::Color textColor = {255, 0, 0};
        int i = 0;
        // Start streaming frames
        while (window.WaitKey('q'))
        {
            // Capture a frame
            FrameData frame = camera.CaptureFrame();
            if (frame.rgbData)
            {
                // Display the frame
                window.ShowFrame(frame.rgbData, frame.width, frame.height);

                CFileImageTag tag(nullptr, 0, 0);
                tag.SetImageId(i);
                i++;
                if (i == 10000)
                    i = 0;
                CImageData data(frame.size,
                                frame.rgbData,
                                frame.width,
                                frame.height,
                                frame.width * 3,
                                IPF_RGB_888,
                                0, &tag);

                fetcher->MyAddImageToBuffer(&data);

                // Draw barcode results on the frame
                {
                    std::lock_guard<std::mutex> lock(textResultsMutex);
                    for (const auto &result : textResults)
                    {
                        if (!result.textLinePoints.empty())
                        {
                            std::vector<std::pair<int, int>> corners = {{result.textLinePoints[0].x, result.textLinePoints[0].y},
                                                                        {result.textLinePoints[1].x, result.textLinePoints[1].y},
                                                                        {result.textLinePoints[2].x, result.textLinePoints[2].y},
                                                                        {result.textLinePoints[3].x, result.textLinePoints[3].y}};
                            window.DrawContour(corners);

                            int x = 20;
                            int y = 40;

                            MRZResult mrzResult = result.info;
                            string msg = "Document Type: " + mrzResult.docType;
                            window.DrawText(msg, x, y, 24, textColor);
                            y += 20;
                            msg = "Document ID: " + mrzResult.docId;
                            window.DrawText(msg, x, y, 24, textColor);
                            y += 20;
                            msg = "Surname: " + mrzResult.surname;
                            window.DrawText(msg, x, y, 24, textColor);
                            y += 20;
                            msg = "Given Name: " + mrzResult.givenname;
                            window.DrawText(msg, x, y, 24, textColor);
                            y += 20;
                            msg = "Nationality: " + mrzResult.nationality;
                            window.DrawText(msg, x, y, 24, textColor);
                            y += 20;
                            msg = "Issuing Country or Organization: " + mrzResult.issuer;
                            window.DrawText(msg, x, y, 24, textColor);
                            y += 20;
                            msg = "Gender: " + mrzResult.gender;
                            window.DrawText(msg, x, y, 24, textColor);
                            y += 20;
                            msg = "Date of Birth(YYMMDD): " + mrzResult.dateOfBirth;
                            window.DrawText(msg, x, y, 24, textColor);
                            y += 20;
                            msg = "Expiration Date(YYMMDD): " + mrzResult.dateOfExpiry;
                        }
                    }
                }

                // Release the frame
                ReleaseFrame(frame);
            }
        }

        camera.Release();
    }

    delete cvr, cvr = NULL;
    delete fetcher, fetcher = NULL;
    delete capturedReceiver, capturedReceiver = NULL;
    std::cout << "Done.\n";
    return 0;
}
/////