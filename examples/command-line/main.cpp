#include <stdio.h>
#include <string>
#include <vector>
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <conio.h>
#include <io.h>
#else
#include <cstring>
#include <dirent.h>
#include <sys/time.h>
#endif

#include <fstream>
#include <streambuf>
#include <iostream>
#include <sstream>

#include "DynamsoftCaptureVisionRouter.h"
#include "DynamsoftUtility.h"

using namespace std;
using namespace dynamsoft::license;
using namespace dynamsoft::cvr;
using namespace dynamsoft::dbr;
using namespace dynamsoft::utility;
using namespace dynamsoft::basic_structures;

class MyCapturedResultReceiver : public CCapturedResultReceiver
{

	virtual void OnDecodedBarcodesReceived(CDecodedBarcodesResult *pResult) override
	{
		if (pResult->GetErrorCode() != EC_OK)
		{
			cout << "Error: " << pResult->GetErrorString() << endl;
		}
		else
		{
			auto tag = pResult->GetOriginalImageTag();
			if (tag)
			{
				cout << "ImageID:" << tag->GetImageId() << endl;
				CFileImageTag *fileTag = (CFileImageTag *)tag;
				cout << "Page number:" << fileTag->GetPageNumber() << endl;
			}
			int count = pResult->GetItemsCount();
			cout << "Decoded " << count << " barcodes" << endl;
			for (int i = 0; i < count; i++)
			{
				const CBarcodeResultItem *barcodeResultItem = pResult->GetItem(i);
				if (barcodeResultItem != NULL)
				{
					cout << "Result " << i + 1 << endl;
					cout << "Barcode Format: " << barcodeResultItem->GetFormatString() << endl;
					cout << "Barcode Text: " << barcodeResultItem->GetText() << endl;
					CPoint *points = barcodeResultItem->GetLocation().points;
					for (int j = 0; j < 4; j++)
					{
						cout << "Point " << j + 1 << ": (" << points[j][0] << ", " << points[j][1] << ")" << endl;
					}
				}
			}
		}

		cout << endl;
	}
};

class MyImageSourceStateListener : public CImageSourceStateListener
{
private:
	CCaptureVisionRouter *m_router;

public:
	MyImageSourceStateListener(CCaptureVisionRouter *router)
	{
		m_router = router;
	}

	virtual void OnImageSourceStateReceived(ImageSourceState state)
	{
		if (state == ISS_EXHAUSTED)
			m_router->StopCapturing();
	}
};

bool GetImagePath(char *pImagePath)
{
	std::string input;
	while (true)
	{
		std::cout << "\n>> Step 1: Input your image file's full path:\n";
		std::getline(std::cin, input);

		// Trim whitespace and remove surrounding quotes if present
		input.erase(0, input.find_first_not_of(" \t\n\r\"\'")); // Trim leading
		input.erase(input.find_last_not_of(" \t\n\r\"\'") + 1); // Trim trailing

		// Exit if user inputs 'q' or 'Q'
		if (input == "q" || input == "Q")
		{
			return true; // Exit flag
		}

		// Copy input to pImagePath ensuring not to exceed buffer size
		std::strncpy(pImagePath, input.c_str(), 511);
		pImagePath[511] = '\0'; // Ensure null-termination

		// Check if file exists using std::ifstream
		std::ifstream file(pImagePath);
		if (file.good())
		{
			file.close();
			return false; // File path is valid
		}

		std::cout << "Please input a valid path.\n";
	}
}

bool endsWith(const std::string &str, const std::string &suffix)
{
	// Check if the suffix length is greater than the string length
	if (suffix.size() > str.size())
	{
		return false;
	}
	// Compare the end of the string with the suffix
	return str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

int main(int argc, char *argv[])
{
	printf("*************************************************\r\n");
	printf("Welcome to Dynamsoft Barcode Demo\r\n");
	printf("*************************************************\r\n");
	printf("Hints: Please input 'Q' or 'q' to quit the application.\r\n");

	int iRet = -1;
	char szErrorMsg[256];
	// Initialize license.
	// Request a trial from https://www.dynamsoft.com/customer/license/trialLicense/?product=dcv&package=cross-platform
	iRet = CLicenseManager::InitLicense("DLS2eyJoYW5kc2hha2VDb2RlIjoiMjAwMDAxLTE2NDk4Mjk3OTI2MzUiLCJvcmdhbml6YXRpb25JRCI6IjIwMDAwMSIsInNlc3Npb25QYXNzd29yZCI6IndTcGR6Vm05WDJrcEQ5YUoifQ==", szErrorMsg, 256);
	if (iRet != EC_OK)
	{
		cout << szErrorMsg << endl;
	}
	int errorCode = 0;
	char errorMsg[512] = {0};

	CCaptureVisionRouter *cvr = new CCaptureVisionRouter;
	errorCode = cvr->InitSettingsFromFile("Templates/DBR-PresetTemplates.json", errorMsg, 512);
	if (errorCode != EC_OK)
	{
		cout << "error:" << errorMsg << endl;
		return -1;
	}

	char pszImageFile[512] = {0};
	bool bExit = false;
	CImageSourceStateListener *listener = new MyImageSourceStateListener(cvr);
	CFileFetcher *fileFetcher = new CFileFetcher();
	cvr->SetInput(fileFetcher);

	CCapturedResultReceiver *capturedReceiver = new MyCapturedResultReceiver;
	cvr->AddResultReceiver(capturedReceiver);
	cvr->AddImageSourceStateListener(listener);

	while (1)
	{
		bExit = GetImagePath(pszImageFile);
		if (bExit)
			break;
		float costTime = 0.0;
		int errorCode = 0;

		string path = string(pszImageFile);
		// fileFetcher->SetFile(pszImageFile);
		// errorCode = cvr->StartCapturing(CPresetTemplate::PT_READ_BARCODES, true, errorMsg, 512);
		// if (endsWith(path, ".pdf") || endsWith(path, ".tif"))
		// {
		// 	fileFetcher->SetFile(pszImageFile);
		// 	errorCode = cvr->StartCapturing(CPresetTemplate::PT_READ_BARCODES, true, errorMsg, 512);
		// }
		// else
		{
			CCapturedResultArray *captureResultArray = cvr->CaptureMultiPages(pszImageFile, CPresetTemplate::PT_READ_BARCODES);

			int count = captureResultArray->GetResultsCount();

			for (int i = 0; i < count; i++)
			{
				CCapturedResult *result = (CCapturedResult *)captureResultArray->GetResult(i);
				result->GetOriginalImageTag()->GetImageId();
				cout << ">>>>>>>>>>>>>>>>> Image " << i + 1 << ":" << endl;
				cout << result->GetErrorString() << endl;
				if (result->GetErrorCode() != 0)
				{
					cout << "Error: " << result->GetErrorCode() << "," << result->GetErrorString() << endl;
				}
				CDecodedBarcodesResult *barcodeResult = result->GetDecodedBarcodesResult();
				if (barcodeResult == nullptr || barcodeResult->GetItemsCount() == 0)
				{
					cout << "No barcode found." << endl;
				}
				else
				{
					int barcodeResultItemCount = barcodeResult->GetItemsCount();
					cout << "Decoded " << barcodeResultItemCount << " barcodes" << endl;

					for (int j = 0; j < barcodeResultItemCount; j++)
					{
						const CBarcodeResultItem *barcodeResultItem = barcodeResult->GetItem(j);
						cout << "Result " << j + 1 << endl;
						cout << "Barcode Format: " << barcodeResultItem->GetFormatString() << endl;
						cout << "Barcode Text: " << barcodeResultItem->GetText() << endl;

						CPoint *points = barcodeResultItem->GetLocation().points;
						for (int j = 0; j < 4; j++)
						{
							cout << "Point " << j + 1 << ": (" << points[j][0] << ", " << points[j][1] << ")" << endl;
						}
					}
				}
				if (barcodeResult)
					barcodeResult->Release();

				result->Release();
			}

			captureResultArray->Release();
		}
	}

	delete cvr, cvr = NULL, listener, capturedReceiver, fileFetcher;
	return 0;
}