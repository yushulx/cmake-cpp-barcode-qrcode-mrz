// Refer to https://github.com/Dynamsoft/barcode-reader-c-cpp-samples/blob/main/Samples/VideoDecoding/VideoDecoding.cpp
#include "opencv2/core.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/videoio.hpp"
#include "opencv2/core/utility.hpp"
#include "opencv2/imgcodecs.hpp"
#include <iostream>
#include <vector>
#include <chrono>
// Include headers of DynamsoftCaptureVisionRouter SDK
#include <iostream>
#include <string>

#include "DynamsoftCaptureVisionRouter.h"
#include "DynamsoftUtility.h"

using namespace std;
using namespace dynamsoft::license;
using namespace dynamsoft::cvr;
using namespace dynamsoft::dlr;
using namespace dynamsoft::dbr;
using namespace dynamsoft::utility;
using namespace dynamsoft::basic_structures;
using namespace cv;

struct BarcodeResult
{
	std::string type;
	std::string value;
	std::vector<cv::Point> localizationPoints;
	int frameId;
	string line;
	std::vector<cv::Point> textLinePoints;
};

std::vector<BarcodeResult> barcodeResults;
std::mutex barcodeResultsMutex;

bool use_ocr = false;

class MyCapturedResultReceiver : public CCapturedResultReceiver
{
	virtual void OnRecognizedTextLinesReceived(CRecognizedTextLinesResult *pResult) override
	{
		if (!use_ocr)
		{
			return;
		}
		std::lock_guard<std::mutex> lock(barcodeResultsMutex);
		barcodeResults.clear();

		const CFileImageTag *tag = dynamic_cast<const CFileImageTag *>(pResult->GetOriginalImageTag());

		// cout << "File: " << tag->GetFilePath() << endl;

		if (pResult->GetErrorCode() != EC_OK)
		{
			cout << "Error: " << pResult->GetErrorString() << endl;
		}
		else
		{
			int lCount = pResult->GetItemsCount();
			for (int li = 0; li < lCount; ++li)
			{
				cout << "Result " << li + 1 << endl;

				BarcodeResult result;

				const CTextLineResultItem *textLine = pResult->GetItem(li);
				CPoint *points = textLine->GetLocation().points;
				result.line = textLine->GetText();
				result.textLinePoints.push_back(cv::Point(points[0][0], points[0][1]));
				result.textLinePoints.push_back(cv::Point(points[1][0], points[1][1]));
				result.textLinePoints.push_back(cv::Point(points[2][0], points[2][1]));
				result.textLinePoints.push_back(cv::Point(points[3][0], points[3][1]));

				CBarcodeResultItem *barcodeResultItem = (CBarcodeResultItem *)textLine->GetReferenceItem();

				points = barcodeResultItem->GetLocation().points;

				result.type = barcodeResultItem->GetFormatString();
				result.value = barcodeResultItem->GetText();
				result.frameId = tag->GetImageId();
				result.localizationPoints.push_back(cv::Point(points[0][0], points[0][1]));
				result.localizationPoints.push_back(cv::Point(points[1][0], points[1][1]));
				result.localizationPoints.push_back(cv::Point(points[2][0], points[2][1]));
				result.localizationPoints.push_back(cv::Point(points[3][0], points[3][1]));

				barcodeResults.push_back(result);

				cout << "Barcode Format: " << barcodeResultItem->GetFormatString() << endl;
				cout << "Barcode Text: " << barcodeResultItem->GetText() << endl;
				cout << ">>Line result " << li << ": " << textLine->GetText() << endl;
			}
		}
	}

	virtual void OnDecodedBarcodesReceived(CDecodedBarcodesResult *pResult) override
	{
		if (use_ocr)
		{
			return;
		}
		std::lock_guard<std::mutex> lock(barcodeResultsMutex);

		if (pResult->GetErrorCode() != EC_OK)
		{
			cout << "Error: " << pResult->GetErrorString() << endl;
		}
		else
		{
			auto tag = pResult->GetOriginalImageTag();
			int count = pResult->GetItemsCount();

			barcodeResults.clear();
			for (int i = 0; i < count; i++)
			{
				const CBarcodeResultItem *barcodeResultItem = pResult->GetItem(i);
				if (barcodeResultItem != NULL)
				{
					cout << "Result " << i + 1 << endl;
					cout << "Barcode Format: " << barcodeResultItem->GetFormatString() << endl;
					cout << "Barcode Text: " << barcodeResultItem->GetText() << endl;
					CPoint *points = barcodeResultItem->GetLocation().points;

					BarcodeResult result;
					result.type = barcodeResultItem->GetFormatString();
					result.value = barcodeResultItem->GetText();
					result.frameId = tag->GetImageId();
					result.localizationPoints.push_back(cv::Point(points[0][0], points[0][1]));
					result.localizationPoints.push_back(cv::Point(points[1][0], points[1][1]));
					result.localizationPoints.push_back(cv::Point(points[2][0], points[2][1]));
					result.localizationPoints.push_back(cv::Point(points[3][0], points[3][1]));

					barcodeResults.push_back(result);
				}
			}
		}

		cout << endl;
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

int main(int argc, char *argv[])
{
	std::string ocr_arg;
	for (int i = 1; i < argc; ++i)
	{
		std::string arg = argv[i];
		if (arg == "--ocr" && i + 1 < argc)
		{
			ocr_arg = argv[++i];
		}
	}

	if (!ocr_arg.empty() && ocr_arg == "true")
	{
		use_ocr = true;
	}

	cout << "Opening camera..." << endl;
	VideoCapture capture(0); // open the first camera
	if (!capture.isOpened())
	{
		cerr << "ERROR: Can't initialize camera capture" << endl;
		cout << "Press any key to quit..." << endl;
		cin.ignore();
		return 1;
	}
	int iRet = -1;
	char szErrorMsg[256];
	// Initialize license.
	// Request a trial from https://www.dynamsoft.com/customer/license/trialLicense/?product=dcv&package=cross-platform
	iRet = CLicenseManager::InitLicense("DLS2eyJoYW5kc2hha2VDb2RlIjoiMjAwMDAxLTE2NDk4Mjk3OTI2MzUiLCJvcmdhbml6YXRpb25JRCI6IjIwMDAwMSIsInNlc3Npb25QYXNzd29yZCI6IndTcGR6Vm05WDJrcEQ5YUoifQ==", szErrorMsg, 256);
	if (iRet != EC_OK)
	{
		cout << szErrorMsg << endl;
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
	// filter->EnableResultDeduplication(CRIT_BARCODE, true);
	// filter->SetDuplicateForgetTime(CRIT_BARCODE | CRIT_TEXT_LINE, 5000);
	cvr->AddResultFilter(filter);

	CCapturedResultReceiver *capturedReceiver = new MyCapturedResultReceiver;
	cvr->AddResultReceiver(capturedReceiver);

	if (strlen(errorMsg) > 0)
	{
		cout << "error:" << errorMsg << ", use_ocr" << use_ocr << endl;
	}

	if (use_ocr)
	{
		// cvr->InitSettings(settings.c_str(), errorMsg, 512);
		// errorCode = cvr->StartCapturing("ReadBarcode&AccompanyText", false, errorMsg, 512);
	}
	else
	{
		errorCode = cvr->StartCapturing("", false, errorMsg, 512);
	}

	if (errorCode != EC_OK)
	{
		cout << "error:" << errorMsg << endl;
	}
	else
	{
		int width = (int)capture.get(CAP_PROP_FRAME_WIDTH);
		int height = (int)capture.get(CAP_PROP_FRAME_HEIGHT);

		for (int i = 1;; ++i)
		{
			Mat frame;
			capture.read(frame);
			if (frame.empty())
			{
				cerr << "ERROR: Can't grab camera frame." << endl;
				break;
			}
			CFileImageTag tag(nullptr, 0, 0);
			tag.SetImageId(i);
			CImageData data(frame.rows * frame.step.p[0],
							frame.data,
							width,
							height,
							frame.step.p[0],
							IPF_RGB_888,
							0,
							&tag);
			fetcher->MyAddImageToBuffer(&data);

			{
				std::lock_guard<std::mutex> lock(barcodeResultsMutex);
				for (const auto &result : barcodeResults)
				{
					// Draw the bounding box
					if (result.localizationPoints.size() == 4)
					{
						for (size_t i = 0; i < result.localizationPoints.size(); ++i)
						{
							cv::line(frame, result.localizationPoints[i],
									 result.localizationPoints[(i + 1) % result.localizationPoints.size()],
									 cv::Scalar(0, 255, 0), 2);
						}
					}

					// Draw the barcode type and value
					if (!result.localizationPoints.empty())
					{
						cv::putText(frame, result.type + ": " + result.value,
									result.localizationPoints[0], cv::FONT_HERSHEY_SIMPLEX,
									0.5, cv::Scalar(0, 255, 0), 2);
					}

					if (!result.line.empty() && !result.textLinePoints.empty())
					{
						for (size_t i = 0; i < result.textLinePoints.size(); ++i)
						{
							cv::line(frame, result.textLinePoints[i],
									 result.textLinePoints[(i + 1) % result.textLinePoints.size()],
									 cv::Scalar(0, 0, 255), 2);
						}

						cv::putText(frame, result.line,
									result.textLinePoints[0], cv::FONT_HERSHEY_SIMPLEX,
									0.5, cv::Scalar(0, 0, 255), 2);
					}
				}
			}

			// Promt to change mode
			cv::putText(frame, "Press 'm' to change mode",
						cv::Point(10, 20), cv::FONT_HERSHEY_SIMPLEX,
						0.5, cv::Scalar(0, 255, 0), 2);

			imshow("1D/2D Barcode Scanner", frame);
			int key = waitKey(1);
			if (key == 27 /*ESC*/)
				break;
			else if (key == 'm' || key == 'M')
			{
				if (!use_ocr)
				{
					// cvr->StopCapturing(false, true);
					// use_ocr = true;
					// cvr->InitSettings(settings.c_str(), errorMsg, 512);
					// errorCode = cvr->StartCapturing("ReadBarcode&AccompanyText", false, errorMsg, 512);
				}
				else
				{
					cvr->StopCapturing(false, true);
					use_ocr = false;
					cvr->ResetSettings();
					errorCode = cvr->StartCapturing(CPresetTemplate::PT_READ_BARCODES, false, errorMsg, 512);
				}
			}
		}
		cvr->StopCapturing(false, true);
	}

	delete cvr, cvr = NULL;
	delete fetcher, fetcher = NULL;
	delete filter, filter = NULL;
	delete capturedReceiver, capturedReceiver = NULL;

	return 0;
}