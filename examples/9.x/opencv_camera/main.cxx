#include "opencv2/core.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/videoio.hpp"
#include "opencv2/core/utility.hpp"
#include "opencv2/imgcodecs.hpp"

#include <iostream>
#include <mutex>
#include <vector>
#include "DynamsoftBarcodeReader.h"

using namespace dynamsoft::dbr;
#if defined(LINUX) || defined(MACOS)
#include <sys/time.h>
#endif

using namespace cv;
using namespace std;
using std::cerr;
using std::cout;
using std::endl;

#define DBR_EXCEPTION(iRet)                                       \
	{                                                             \
		if (iRet != 0)                                            \
		{                                                         \
			printf("%s\n", CBarcodeReader::GetErrorString(iRet)); \
			return -1;                                            \
		}                                                         \
	}

struct BarcodeResult
{
	std::string type;
	std::string value;
	std::vector<cv::Point> localizationPoints;
	int frameId;
};

std::vector<BarcodeResult> barcodeResults;
std::mutex barcodeResultsMutex;

void ToHexString(unsigned char *pSrc, int iLen, char *pDest)
{
	const char HEXCHARS[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

	int i;
	char *ptr = pDest;

	for (i = 0; i < iLen; ++i)
	{
		snprintf(ptr, 4, "%c%c ", HEXCHARS[(pSrc[i] & 0xF0) >> 4], HEXCHARS[(pSrc[i] & 0x0F) >> 0]);
		ptr += 3;
	}
}

void textResultCallback(int frameId, TextResultArray *pResults, void *pUser)
{
	std::lock_guard<std::mutex> lock(barcodeResultsMutex);
	barcodeResults.clear();

	if (pResults->resultsCount == 0)
	{
		printf("No barcode found.\r\n\r\n");
		CBarcodeReader::FreeTextResults(&pResults);
		return;
	}

	printf("\r\n\r\nframe = %d,Total barcode(s) found: %d.\n", frameId, pResults->resultsCount);
	for (int iIndex = 0; iIndex < pResults->resultsCount; iIndex++)
	{
		LocalizationResult *localizationResult = pResults->results[iIndex]->localizationResult;
		printf("Barcode %d:\r\n", iIndex + 1);
		printf("    Type: %s\r\n", pResults->results[iIndex]->barcodeFormatString);
		printf("    Value: %s\r\n", pResults->results[iIndex]->barcodeText);
		printf("x1: %d, y1: %d, x2: %d, y2: %d, x3: %d, y3: %d, x4: %d, y4: %d\r\n", localizationResult->x1, localizationResult->y1, localizationResult->x2, localizationResult->y2, localizationResult->x3, localizationResult->y3, localizationResult->x4, localizationResult->y4);

		// Save results in shared data structure
		BarcodeResult result;
		result.type = pResults->results[iIndex]->barcodeFormatString;
		result.value = pResults->results[iIndex]->barcodeText;
		result.frameId = frameId;
		result.localizationPoints.push_back(cv::Point(localizationResult->x1, localizationResult->y1));
		result.localizationPoints.push_back(cv::Point(localizationResult->x2, localizationResult->y2));
		result.localizationPoints.push_back(cv::Point(localizationResult->x3, localizationResult->y3));
		result.localizationPoints.push_back(cv::Point(localizationResult->x4, localizationResult->y4));

		barcodeResults.push_back(result);
	}

	CBarcodeReader::FreeTextResults(&pResults);
}

void errorcb(int frameId, int errorCode, void *pUser)
{
	std::lock_guard<std::mutex> lock(barcodeResultsMutex);
	if (barcodeResults.size() > 0 && barcodeResults[0].frameId != frameId)
	{
		barcodeResults.clear();
	}

	if (errorCode != DBR_OK && errorCode != DBRERR_LICENSE_EXPIRED && errorCode != DBRERR_QR_LICENSE_INVALID &&
		errorCode != DBRERR_1D_LICENSE_INVALID && errorCode != DBRERR_PDF417_LICENSE_INVALID && errorCode != DBRERR_DATAMATRIX_LICENSE_INVALID)
	{
		printf("Failed to read barcode: %s\r\n", CBarcodeReader::GetErrorString(errorCode));
		return;
	}
}

int main()
{
	// getchar();
	Mat frame;
	cout << "Opening camera..." << endl;
	VideoCapture capture(0); // open the first camera
	if (!capture.isOpened())
	{
		cerr << "ERROR: Can't initialize camera capture" << endl;
		return 1;
	}

	int iRet = -1;
	CBarcodeReader reader;
	iRet = reader.InitLicense("LICENSE-KEY");
	if (iRet != 0)
	{
		printf("%s\n", CBarcodeReader::GetErrorString(iRet));
	}
	PublicRuntimeSettings runtimeSettings;
	char szErrorMsg[256];
	// Best coverage settings
	reader.InitRuntimeSettingsWithString("{\"ImageParameter\":{\"Name\":\"BestCoverage\",\"DeblurLevel\":9,\"ExpectedBarcodesCount\":512,\"ScaleDownThreshold\":100000,\"LocalizationModes\":[{\"Mode\":\"LM_CONNECTED_BLOCKS\"},{\"Mode\":\"LM_SCAN_DIRECTLY\"},{\"Mode\":\"LM_STATISTICS\"},{\"Mode\":\"LM_LINES\"},{\"Mode\":\"LM_STATISTICS_MARKS\"}],\"GrayscaleTransformationModes\":[{\"Mode\":\"GTM_ORIGINAL\"},{\"Mode\":\"GTM_INVERTED\"}]}}", CM_OVERWRITE, szErrorMsg, 256);
	// Best speed settings
	// reader.InitRuntimeSettingsWithString("{\"ImageParameter\":{\"Name\":\"BestSpeed\",\"DeblurLevel\":3,\"ExpectedBarcodesCount\":512,\"LocalizationModes\":[{\"Mode\":\"LM_SCAN_DIRECTLY\"}],\"TextFilterModes\":[{\"MinImageDimension\":262144,\"Mode\":\"TFM_GENERAL_CONTOUR\"}]}}",CM_OVERWRITE,szErrorMsg,256);
	// Balance settings
	// reader.InitRuntimeSettingsWithString("{\"ImageParameter\":{\"Name\":\"Balance\",\"DeblurLevel\":5,\"ExpectedBarcodesCount\":512,\"LocalizationModes\":[{\"Mode\":\"LM_CONNECTED_BLOCKS\"},{\"Mode\":\"LM_STATISTICS\"}]}}",CM_OVERWRITE,szErrorMsg,256);
	reader.GetRuntimeSettings(&runtimeSettings);
	runtimeSettings.barcodeFormatIds = BF_ALL;
	runtimeSettings.barcodeFormatIds_2 = BF2_POSTALCODE | BF2_DOTCODE;
	runtimeSettings.intermediateResultTypes = IRT_ORIGINAL_IMAGE;
	DBR_EXCEPTION(reader.UpdateRuntimeSettings(&runtimeSettings, szErrorMsg, 256));
	DBR_EXCEPTION(reader.SetTextResultCallback(textResultCallback, NULL));
	DBR_EXCEPTION(reader.SetErrorCallback(errorcb, NULL));
	capture >> frame;
	int width = capture.get(CAP_PROP_FRAME_WIDTH);
	int height = capture.get(CAP_PROP_FRAME_HEIGHT);
	int format = capture.get(CAP_PROP_FORMAT);
	int t = frame.type();
	int depth = frame.depth();
	int chnnel = frame.channels();
	iRet = reader.StartFrameDecoding(30, 30, width, height, frame.step.p[0], IPF_RGB_888, "");
	if (iRet != 0)
	{
		printf("%s\n", CBarcodeReader::GetErrorString(iRet));
		return -1;
	}
	for (;;)
	{
		capture >> frame; // read the next frame from camera
		if (frame.empty())
		{
			cerr << "ERROR: Can't grab camera frame." << endl;
			break;
		}
		reader.AppendFrame(frame.data);

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
			}
		}

		imshow("BarcodeReader", frame);
		int key = waitKey(1);
		if (key == 27 /*ESC*/)
			break;
	}

	reader.StopFrameDecoding();

	return 0;
}