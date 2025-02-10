#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>

#include "template.h"
#include "DynamsoftCaptureVisionRouter.h"
#include "DynamsoftUtility.h"
#include "Camera.h"
#include "CameraPreview.h"

using namespace dynamsoft::license;
using namespace dynamsoft::cvr;
using namespace dynamsoft::dbr;
using namespace dynamsoft::utility;
using namespace dynamsoft::basic_structures;

// Thread-safe queue for processing results
template <typename T>
class ThreadSafeQueue
{
private:
    std::queue<T> queue;
    std::mutex mtx;
    std::condition_variable cv;

public:
    void Push(const T &item)
    {
        std::lock_guard<std::mutex> lock(mtx);
        queue.push(item);
        cv.notify_one();
    }

    bool Pop(T &item)
    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this]()
                { return !queue.empty(); });

        if (!queue.empty())
        {
            item = queue.front();
            queue.pop();
            return true;
        }
        return false;
    }

    bool Empty()
    {
        std::lock_guard<std::mutex> lock(mtx);
        return queue.empty();
    }
};

// Structure to store processing results
struct ProcessingResult
{
    std::vector<std::vector<std::pair<int, int>>> contours;
    std::vector<std::string> texts;
};

// Global running flag
std::atomic<bool> running{true};

void ProcessFrames(CCaptureVisionRouter *cvr, ThreadSafeQueue<FrameData> &frameQueue, ThreadSafeQueue<ProcessingResult> &resultQueue)
{
    while (running)
    {
        FrameData frame;
        if (frameQueue.Pop(frame))
        {
            if (frame.rgbData == nullptr)
            {
                continue;
            }

            ProcessingResult result;

            CImageData data(frame.size,
                            frame.rgbData,
                            frame.width,
                            frame.height,
                            frame.width * 3,
                            IPF_RGB_888,
                            0, 0);
            CCapturedResult *captureResult = cvr->Capture(&data, "");

            if (captureResult)
            {
                CDecodedBarcodesResult *barcodeResult = captureResult->GetDecodedBarcodesResult();
                if (barcodeResult)
                {
                    int count = barcodeResult->GetItemsCount();
                    for (int i = 0; i < count; i++)
                    {
                        const CBarcodeResultItem *barcodeResultItem = barcodeResult->GetItem(i);
                        CPoint *points = barcodeResultItem->GetLocation().points;

                        std::vector<std::pair<int, int>> corners = {
                            {points[0][0], points[0][1]},
                            {points[1][0], points[1][1]},
                            {points[2][0], points[2][1]},
                            {points[3][0], points[3][1]}};

                        result.contours.push_back(corners);
                        result.texts.push_back(barcodeResultItem->GetText());
                    }
                }

                resultQueue.Push(result);

                captureResult->Release();
            }

            ReleaseFrame(frame);
        }
    }
}

int main()
{
    // Initialize license
    char szErrorMsg[256];
    if (CLicenseManager::InitLicense("LICENSE-KEY", szErrorMsg, 256) != EC_OK)
    {
        std::cerr << szErrorMsg << std::endl;
        return -1;
    }

    // Initialize Capture Vision Router
    CCaptureVisionRouter *cvr = new CCaptureVisionRouter;
    if (cvr->InitSettings(jsonString.c_str(), szErrorMsg, 256) != EC_OK)
    {
        std::cerr << "Error initializing CVR: " << szErrorMsg << std::endl;
        return -1;
    }

    // Instantiate Camera
    Camera camera;
    if (!camera.Open(0))
    {
        std::cerr << "Failed to open camera." << std::endl;
        return -1;
    }

    // Instantiate Camera Window
    CameraWindow window(camera.frameWidth, camera.frameHeight, "Camera Stream");
    if (!window.Create())
    {
        std::cerr << "Failed to create window." << std::endl;
        return -1;
    }
    window.Show();

    // Thread-safe frame and result queues
    ThreadSafeQueue<FrameData> frameQueue;
    ThreadSafeQueue<ProcessingResult> resultQueue;

    // Start processing thread
    std::thread processingThread(ProcessFrames, cvr, std::ref(frameQueue), std::ref(resultQueue));

    // Main loop for frame capture and drawing
    CameraWindow::Color textColor = {255, 0, 0};

    while (window.WaitKey('q'))
    {
        // Capture a frame
        FrameData frame = camera.CaptureFrame();
        if (frame.rgbData)
        {
            // Display the frame
            window.ShowFrame(frame.rgbData, frame.width, frame.height);

            // Push frame to processing queue
            frameQueue.Push(frame);

            // Draw contours and texts if results are available
            ProcessingResult result;
            if (!resultQueue.Empty() && resultQueue.Pop(result))
            {
                for (size_t i = 0; i < result.contours.size(); ++i)
                {
                    window.DrawContour(result.contours[i]);
                    window.DrawText(result.texts[i], result.contours[i][0].first, result.contours[i][0].second, 24, textColor);
                }
            }
        }
    }

    running = false;

    // Wait for processing thread to finish
    processingThread.join();

    camera.Release();
    delete cvr;

    return 0;
}
