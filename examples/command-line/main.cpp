#include <stdio.h>
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
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
					CQuadrilateral location = barcodeResultItem->GetLocation();
					CPoint *points = location.points;
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
		std::cout << "\n>> Step 1: Input your image file's full path or directory path:\n";
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
		if (input.length() < 512)
		{
			strcpy_s(pImagePath, 512, input.c_str());
		}
		else
		{
			input = input.substr(0, 511); // Truncate if too long
			strcpy_s(pImagePath, 512, input.c_str());
		}

		// Check if file or directory exists
		if (filesystem::exists(pImagePath))
		{
			return false; // Path is valid
		}

		std::cout << "Please input a valid file or directory path.\n";
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

// Structure to hold processing statistics
struct ProcessingStats
{
	int totalImages = 0;
	int imagesWithBarcodes = 0;
	int totalBarcodes = 0;
	vector<string> failedFiles;
};

// Check if a file is a supported image format
bool isSupportedImageFile(const string &filePath)
{
	string extension = filesystem::path(filePath).extension().string();
	transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

	return extension == ".jpg" || extension == ".jpeg" || extension == ".png" ||
		   extension == ".bmp" || extension == ".tif" || extension == ".tiff" ||
		   extension == ".gif" || extension == ".pdf";
}

// Get all supported image files from a directory
vector<string> getImageFilesFromDirectory(const string &dirPath)
{
	vector<string> imageFiles;

	try
	{
		for (const auto &entry : filesystem::recursive_directory_iterator(dirPath))
		{
			if (entry.is_regular_file() && isSupportedImageFile(entry.path().string()))
			{
				imageFiles.push_back(entry.path().string());
			}
		}
	}
	catch (const filesystem::filesystem_error &e)
	{
		cout << "Error accessing directory: " << e.what() << endl;
	}

	return imageFiles;
}

// Process a single image file and return the number of barcodes found
int processImageFile(CCaptureVisionRouter *cvr, const string &filePath)
{
	CCapturedResultArray *captureResultArray = cvr->CaptureMultiPages(filePath.c_str(), CPresetTemplate::PT_READ_BARCODES);

	int totalBarcodes = 0;
	int count = captureResultArray->GetResultsCount();

	for (int i = 0; i < count; i++)
	{
		const CCapturedResult *result = captureResultArray->GetResult(i);

		if (result->GetErrorCode() != 0)
		{
			continue;
		}

		CDecodedBarcodesResult *barcodeResult = result->GetDecodedBarcodesResult();
		if (barcodeResult != nullptr && barcodeResult->GetErrorCode() == 0)
		{
			int itemCount = barcodeResult->GetItemsCount();
			totalBarcodes += itemCount;
		}
		if (barcodeResult)
			barcodeResult->Release();
	}

	captureResultArray->Release();
	return totalBarcodes;
}

// Display detailed results for a single image (original behavior)
void displayDetailedResults(CCaptureVisionRouter *cvr, const string &filePath)
{
	cout << "Processing file: " << filePath << endl;

	CCapturedResultArray *captureResultArray = cvr->CaptureMultiPages(filePath.c_str(), CPresetTemplate::PT_READ_BARCODES);

	int count = captureResultArray->GetResultsCount();

	for (int i = 0; i < count; i++)
	{
		const CCapturedResult *result = captureResultArray->GetResult(i);
		result->GetOriginalImageTag()->GetImageId();
		cout << ">>>>>>>>>>>>>>>>> Image " << i + 1 << ":" << endl;
		cout << result->GetErrorString() << endl;
		if (result->GetErrorCode() != 0)
			continue;

		CDecodedBarcodesResult *barcodeResult = result->GetDecodedBarcodesResult();
		if (barcodeResult != nullptr && barcodeResult->GetErrorCode() == 0)
		{
			int itemCount = barcodeResult->GetItemsCount();
			cout << "Decoded " << itemCount << " barcodes" << endl;
			for (int j = 0; j < itemCount; j++)
			{
				const CBarcodeResultItem *barcodeResultItem = barcodeResult->GetItem(j);
				cout << "Result " << j + 1 << endl;
				cout << "Barcode Format: " << barcodeResultItem->GetFormatString() << endl;
				cout << "Barcode Text: " << barcodeResultItem->GetText() << endl;

				CPoint *points = barcodeResultItem->GetLocation().points;
				for (int k = 0; k < 4; k++)
				{
					cout << "Point " << k + 1 << ": (" << points[k][0] << ", " << points[k][1] << ")" << endl;
				}
			}
		}
		if (barcodeResult)
			barcodeResult->Release();
	}
	captureResultArray->Release();
}

int main(int argc, char *argv[])
{
	const char *version = CBarcodeReaderModule::GetVersion();
	printf("*************************************************\r\n");
	printf("Welcome to Dynamsoft Barcode v%s Demo \r\n", version);
	printf("*************************************************\r\n");
	printf("Supports both single files and directories.\r\n");
	printf("Usage: %s [file_or_directory1] [file_or_directory2] ...\r\n", argc > 0 ? argv[0] : "barcode_scanner");
	printf("Or run without arguments for interactive mode.\r\n");
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

	// Check if command line arguments are provided
	if (argc > 1)
	{
		// Command line mode - process all provided arguments
		ProcessingStats totalStats;

		for (int argIndex = 1; argIndex < argc; argIndex++)
		{
			string inputPath = argv[argIndex];

			if (filesystem::is_directory(inputPath))
			{
				// Process directory
				cout << "\n=== Processing Directory: " << inputPath << " ===" << endl;

				vector<string> imageFiles = getImageFilesFromDirectory(inputPath);
				ProcessingStats dirStats;
				dirStats.totalImages = imageFiles.size();

				if (imageFiles.empty())
				{
					cout << "No supported image files found in directory." << endl;
					continue;
				}

				cout << "Found " << imageFiles.size() << " image files. Processing..." << endl;

				for (const string &filePath : imageFiles)
				{
					try
					{
						int barcodeCount = processImageFile(cvr, filePath);
						if (barcodeCount > 0)
						{
							dirStats.imagesWithBarcodes++;
							dirStats.totalBarcodes += barcodeCount;
							cout << filesystem::path(filePath).filename().string()
								 << " (" << barcodeCount << " barcodes)" << endl;
						}
						else
						{
							cout << filesystem::path(filePath).filename().string()
								 << " (no barcodes)" << endl;
						}
					}
					catch (const exception &e)
					{
						dirStats.failedFiles.push_back(filePath);
						cout << "✗ " << filesystem::path(filePath).filename().string()
							 << " (error: " << e.what() << ")" << endl;
					}
				}

				// Display directory statistics
				cout << "\n--- Directory Statistics ---" << endl;
				cout << "Total images processed: " << dirStats.totalImages << endl;
				cout << "Images with barcodes: " << dirStats.imagesWithBarcodes << endl;
				cout << "Images without barcodes: " << (dirStats.totalImages - dirStats.imagesWithBarcodes - dirStats.failedFiles.size()) << endl;
				if (!dirStats.failedFiles.empty())
				{
					cout << "Failed to process: " << dirStats.failedFiles.size() << endl;
				}
				cout << "Total barcodes found: " << dirStats.totalBarcodes << endl;
				if (dirStats.totalImages > 0)
				{
					cout << "Success rate: " << (double(dirStats.imagesWithBarcodes) / dirStats.totalImages * 100.0) << "%" << endl;
				}

				// Add to total stats
				totalStats.totalImages += dirStats.totalImages;
				totalStats.imagesWithBarcodes += dirStats.imagesWithBarcodes;
				totalStats.totalBarcodes += dirStats.totalBarcodes;
				totalStats.failedFiles.insert(totalStats.failedFiles.end(),
											  dirStats.failedFiles.begin(), dirStats.failedFiles.end());
			}
			else if (filesystem::is_regular_file(inputPath) && isSupportedImageFile(inputPath))
			{
				// Process single file with detailed output
				cout << "\n=== Processing File: " << inputPath << " ===" << endl;
				displayDetailedResults(cvr, inputPath);

				// Update total stats
				totalStats.totalImages++;
				int barcodeCount = processImageFile(cvr, inputPath);
				if (barcodeCount > 0)
				{
					totalStats.imagesWithBarcodes++;
					totalStats.totalBarcodes += barcodeCount;
				}
			}
			else
			{
				cout << "Warning: '" << inputPath << "' is not a valid file or directory, or not a supported image format." << endl;
			}
		}

		// Display overall statistics if multiple inputs were processed
		if (argc > 2)
		{
			cout << "\n=== Overall Statistics ===" << endl;
			cout << "Total images processed: " << totalStats.totalImages << endl;
			cout << "Images with barcodes: " << totalStats.imagesWithBarcodes << endl;
			cout << "Images without barcodes: " << (totalStats.totalImages - totalStats.imagesWithBarcodes - totalStats.failedFiles.size()) << endl;
			if (!totalStats.failedFiles.empty())
			{
				cout << "Failed to process: " << totalStats.failedFiles.size() << endl;
			}
			cout << "Total barcodes found: " << totalStats.totalBarcodes << endl;
			if (totalStats.totalImages > 0)
			{
				cout << "Overall success rate: " << (double(totalStats.imagesWithBarcodes) / totalStats.totalImages * 100.0) << "%" << endl;
			}
		}
	}
	else
	{
		// Interactive mode - original behavior with directory support
		while (1)
		{
			bExit = GetImagePath(pszImageFile);
			if (bExit)
				break;

			string inputPath = string(pszImageFile);

			if (filesystem::is_directory(inputPath))
			{
				// Process directory in interactive mode
				cout << "\nProcessing directory: " << inputPath << endl;

				vector<string> imageFiles = getImageFilesFromDirectory(inputPath);
				ProcessingStats dirStats;
				dirStats.totalImages = imageFiles.size();

				if (imageFiles.empty())
				{
					cout << "No supported image files found in directory." << endl;
					continue;
				}

				cout << "Found " << imageFiles.size() << " image files. Processing..." << endl;

				for (const string &filePath : imageFiles)
				{
					try
					{
						int barcodeCount = processImageFile(cvr, filePath);
						if (barcodeCount > 0)
						{
							dirStats.imagesWithBarcodes++;
							dirStats.totalBarcodes += barcodeCount;
							cout << "✓ " << filesystem::path(filePath).filename().string()
								 << " (" << barcodeCount << " barcodes)" << endl;
						}
						else
						{
							cout << "✗ " << filesystem::path(filePath).filename().string()
								 << " (no barcodes)" << endl;
						}
					}
					catch (const exception &e)
					{
						dirStats.failedFiles.push_back(filePath);
						cout << "✗ " << filesystem::path(filePath).filename().string()
							 << " (error: " << e.what() << ")" << endl;
					}
				}

				// Display directory statistics
				cout << "\n--- Directory Statistics ---" << endl;
				cout << "Total images processed: " << dirStats.totalImages << endl;
				cout << "Images with barcodes: " << dirStats.imagesWithBarcodes << endl;
				cout << "Images without barcodes: " << (dirStats.totalImages - dirStats.imagesWithBarcodes - dirStats.failedFiles.size()) << endl;
				if (!dirStats.failedFiles.empty())
				{
					cout << "Failed to process: " << dirStats.failedFiles.size() << endl;
				}
				cout << "Total barcodes found: " << dirStats.totalBarcodes << endl;
				if (dirStats.totalImages > 0)
				{
					cout << "Success rate: " << (double(dirStats.imagesWithBarcodes) / dirStats.totalImages * 100.0) << "%" << endl;
				}
			}
			else
			{
				// Process single file with detailed output (original behavior)
				displayDetailedResults(cvr, inputPath);
			}
		}
	}

	delete cvr, cvr = NULL, listener, capturedReceiver, fileFetcher;
	return 0;
}