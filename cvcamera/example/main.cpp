#include <iostream>
#include <vector>
#include <mutex>
#include "camera_lib.h"

#include "DynamsoftCaptureVisionRouter.h"
#include "DynamsoftUtility.h"

using namespace dynamsoft::license;
using namespace dynamsoft::cvr;
using namespace dynamsoft::dbr;
using namespace dynamsoft::utility;
using namespace dynamsoft::basic_structures;

struct BarcodeResult
{
    std::string type;
    std::string value;
    int x1, y1, x2, y2, x3, y3, x4, y4;
    int frameId;
};

std::vector<BarcodeResult> barcodeResults;
std::mutex barcodeResultsMutex;

class MyCapturedResultReceiver : public CCapturedResultReceiver
{
    virtual void OnDecodedBarcodesReceived(CDecodedBarcodesResult *pResult) override
    {
        std::lock_guard<std::mutex> lock(barcodeResultsMutex);

        if (pResult->GetErrorCode() != EC_OK)
        {
            std::cout << "Error: " << pResult->GetErrorString() << std::endl;
        }
        else
        {
            auto tag = pResult->GetOriginalImageTag();
            if (tag)
                std::cout << "ImageID:" << tag->GetImageId() << std::endl;
            int count = pResult->GetItemsCount();
            std::cout << "Decoded " << count << " barcodes" << std::endl;

            barcodeResults.clear();
            for (int i = 0; i < count; i++)
            {
                const CBarcodeResultItem *barcodeResultItem = pResult->GetItem(i);
                if (barcodeResultItem != NULL)
                {
                    std::cout << "Result " << i + 1 << std::endl;
                    std::cout << "Barcode Format: " << barcodeResultItem->GetFormatString() << std::endl;
                    std::cout << "Barcode Text: " << barcodeResultItem->GetText() << std::endl;
                    CPoint *points = barcodeResultItem->GetLocation().points;

                    BarcodeResult result;
                    result.type = barcodeResultItem->GetFormatString();
                    result.value = barcodeResultItem->GetText();
                    result.frameId = tag->GetImageId();
                    result.x1 = points[0][0];
                    result.y1 = points[0][1];
                    result.x2 = points[1][0];
                    result.y2 = points[1][1];
                    result.x3 = points[2][0];
                    result.y3 = points[2][1];
                    result.x4 = points[3][0];
                    result.y4 = points[3][1];

                    barcodeResults.push_back(result);
                }
            }
        }

        std::cout << std::endl;
    }
};

class MyVideoFetcher : public CImageSourceAdapter
{
public:
    MyVideoFetcher() {};
    ~MyVideoFetcher() {};
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
    if (open_camera(0) == 0)
    {
        std::cout << "Camera opened successfully." << std::endl;

        int iRet = -1;
        char szErrorMsg[256];
        // Initialize license.
        // Request a trial from https://www.dynamsoft.com/customer/license/trialLicense/?product=dcv&package=cross-platform
        iRet = CLicenseManager::InitLicense("LICENSE-KEY", szErrorMsg, 256);
        if (iRet != EC_OK)
        {
            std::cout << szErrorMsg << std::endl;
        }
        int errorCode = 1;
        char errorMsg[512] = {0};

        CCaptureVisionRouter *cvr = new CCaptureVisionRouter;

        MyVideoFetcher *fetcher = new MyVideoFetcher();
        fetcher->SetMaxImageCount(4);
        fetcher->SetBufferOverflowProtectionMode(BOPM_UPDATE);
        fetcher->SetColourChannelUsageType(CCUT_AUTO);
        cvr->SetInput(fetcher);

        CMultiFrameResultCrossFilter *filter = new CMultiFrameResultCrossFilter;
        filter->EnableResultCrossVerification(CRIT_BARCODE | CRIT_TEXT_LINE, true);
        cvr->AddResultFilter(filter);

        CCapturedResultReceiver *capturedReceiver = new MyCapturedResultReceiver;
        cvr->AddResultReceiver(capturedReceiver);

        errorCode = cvr->StartCapturing(CPresetTemplate::PT_READ_BARCODES, false, errorMsg, 512);
        if (errorCode != EC_OK)
        {
            std::cout << "error:" << errorMsg << std::endl;
        }

        for (int i = 1;; ++i)
        {
            ImageData frame = get_frame();
            if (frame.data)
            {
                CFileImageTag tag(nullptr, 0, 0);
                tag.SetImageId(i);
                CImageData data(frame.height * frame.stride,
                                frame.data,
                                frame.width,
                                frame.height,
                                frame.stride,
                                IPF_RGB_888,
                                0,
                                &tag);
                fetcher->MyAddImageToBuffer(&data);

                {
                    std::lock_guard<std::mutex> lock(barcodeResultsMutex);
                    for (const auto &result : barcodeResults)
                    {
                        // Draw the bounding box
                        draw_line(&frame, result.x1, result.y1, result.x2, result.y2, 2, 0, 255, 0);
                        draw_line(&frame, result.x2, result.y2, result.x3, result.y3, 2, 0, 255, 0);
                        draw_line(&frame, result.x3, result.y3, result.x4, result.y4, 2, 0, 255, 0);
                        draw_line(&frame, result.x4, result.y4, result.x1, result.y1, 2, 0, 255, 0);

                        // Draw the barcode type and value
                        std::string text = result.type + ": " + result.value;
                        draw_text(&frame, text.c_str(), result.x1, result.y1 - 10, 1, 2, 0, 255, 0);
                    }
                }

                display_frame("1D/2D Barcode Scanner", &frame);
                if (wait_key(30) >= 0)
                { // Add a delay and check for key press
                    release_frame(&frame);
                    break; // Exit the loop if any key is pressed
                }
                release_frame(&frame);
            }
        }
        close_camera();
    }
    else
    {
        std::cerr << "Failed to open camera." << std::endl;
    }
    return 0;
}
