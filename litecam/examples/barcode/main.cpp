#include <iostream>
#include <vector>
#include <mutex>

#include "DynamsoftCaptureVisionRouter.h"
#include "DynamsoftUtility.h"
#include "Camera.h"
#include "CameraPreview.h"

using namespace dynamsoft::license;
using namespace dynamsoft::cvr;
using namespace dynamsoft::dbr;
using namespace dynamsoft::utility;
using namespace dynamsoft::basic_structures;

int main()
{
    int iRet = -1;
    char szErrorMsg[256];
    // Initialize license.
    // Request a trial from https://www.dynamsoft.com/customer/license/trialLicense/?product=dcv&package=cross-platform
    iRet = CLicenseManager::InitLicense("DLS2eyJoYW5kc2hha2VDb2RlIjoiMjAwMDAxLTE2NDk4Mjk3OTI2MzUiLCJvcmdhbml6YXRpb25JRCI6IjIwMDAwMSIsInNlc3Npb25QYXNzd29yZCI6IndTcGR6Vm05WDJrcEQ5YUoifQ==", szErrorMsg, 256);
    if (iRet != EC_OK)
    {
        std::cout << szErrorMsg << std::endl;
    }
    int errorCode = 1;
    char errorMsg[512] = {0};

    CCaptureVisionRouter *cvr = new CCaptureVisionRouter;
    // errorCode = cvr->InitSettings(jsonString.c_str(), errorMsg, 512);
    // if (errorCode != EC_OK)
    // {
    //     std::cout << "error:" << errorMsg << std::endl;
    //     return -1;
    // }

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

        // Start streaming frames
        CameraWindow::Color textColor = {255, 0, 0};

        while (window.WaitKey('q'))
        {
            // Capture a frame
            FrameData frame = camera.CaptureFrame();

            if (frame.rgbData)
            {
                // Display the frame
                window.ShowFrame(frame.rgbData, frame.width, frame.height);

                CImageData data(frame.size,
                                frame.rgbData,
                                frame.width,
                                frame.height,
                                frame.width * 3,
                                IPF_RGB_888,
                                0, 0);
                CCapturedResult *result = cvr->Capture(&data, CPresetTemplate::PT_READ_BARCODES);

                if (result)
                {
                    CDecodedBarcodesResult *barcodeResult = result->GetDecodedBarcodesResult();
                    if (barcodeResult)
                    {
                        int count = barcodeResult->GetItemsCount();
                        for (int i = 0; i < count; i++)
                        {
                            const CBarcodeResultItem *barcodeResultItem = barcodeResult->GetItem(i);
                            CPoint *points = barcodeResultItem->GetLocation().points;
                            int x = points[0][0];
                            int y = points[0][1];

                            std::vector<std::pair<int, int>> corners = {
                                {points[0][0], points[0][1]},
                                {points[1][0], points[1][1]},
                                {points[2][0], points[2][1]},
                                {points[3][0], points[3][1]}};

                            window.DrawContour(corners);

                            window.DrawText(barcodeResultItem->GetText(), x, y, 24, textColor);
                        }
                    }

                    result->Release();
                }
            }

            ReleaseFrame(frame);
        }

        camera.Release();
    }

    delete cvr;
    std::cout << "Done.\n";
    return 0;
}
