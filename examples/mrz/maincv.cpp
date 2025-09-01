#include "opencv2/core.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/videoio.hpp"
#include "opencv2/core/utility.hpp"
#include "opencv2/imgcodecs.hpp"
#include <iostream>
#include <vector>
#include <chrono>
#include <iostream>
#include <string>

#include "DynamsoftCaptureVisionRouter.h"
#include "DynamsoftUtility.h"

using namespace std;
using namespace cv;

using namespace dynamsoft::cvr;
using namespace dynamsoft::dlr;
using namespace dynamsoft::dcp;
using namespace dynamsoft::license;
using namespace dynamsoft::basic_structures;
using namespace dynamsoft::utility;

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

struct TextResult
{
	int id;
	MRZResult info;
	std::vector<cv::Point> textLinePoints;
};

std::vector<TextResult> textResults;
std::mutex textResultsMutex;

class MyCapturedResultReceiver : public CCapturedResultReceiver
{
	virtual void OnRecognizedTextLinesReceived(CRecognizedTextLinesResult *pResult) override
	{
		std::lock_guard<std::mutex> lock(textResultsMutex);
		textResults.clear();

		const CImageTag *tag = pResult->GetOriginalImageTag();

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
				result.textLinePoints.push_back(cv::Point(points[0][0], points[0][1]));
				result.textLinePoints.push_back(cv::Point(points[1][0], points[1][1]));
				result.textLinePoints.push_back(cv::Point(points[2][0], points[2][1]));
				result.textLinePoints.push_back(cv::Point(points[3][0], points[3][1]));

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

void drawText(Mat &img, const char *text, int x, int y)
{
	putText(img, text, Point(x, y), cv::FONT_HERSHEY_SIMPLEX,
			0.5, cv::Scalar(0, 0, 255), 1);
}

int main(int argc, char *argv[])
{
	bool captured = false;
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
	iRet = CLicenseManager::InitLicense("LICENSE-KEY", szErrorMsg, 256);
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

	CCapturedResultReceiver *capturedReceiver = new MyCapturedResultReceiver;
	cvr->AddResultReceiver(capturedReceiver);

	// errorCode = cvr->InitSettingsFromFile("MRZ.json", errorMsg, 512);
	// if (errorCode != EC_OK)
	// {
	// 	cout << "error:" << errorMsg << endl;
	// }

	errorCode = cvr->StartCapturing("ReadPassportAndId", false, errorMsg, 512);

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
				std::lock_guard<std::mutex> lock(textResultsMutex);
				for (const auto &result : textResults)
				{
					if (!result.textLinePoints.empty())
					{
						for (size_t i = 0; i < result.textLinePoints.size(); ++i)
						{
							cv::line(frame, result.textLinePoints[i],
									 result.textLinePoints[(i + 1) % result.textLinePoints.size()],
									 cv::Scalar(0, 0, 255), 2);
						}

						int x = 20;
						int y = 40;

						MRZResult mrzResult = result.info;
						string msg = "Document Type: " + mrzResult.docType;
						drawText(frame, msg.c_str(), x, y);
						y += 20;
						msg = "Document ID: " + mrzResult.docId;
						drawText(frame, msg.c_str(), x, y);
						y += 20;
						msg = "Surname: " + mrzResult.surname;
						drawText(frame, msg.c_str(), x, y);
						y += 20;
						msg = "Given Name: " + mrzResult.givenname;
						drawText(frame, msg.c_str(), x, y);
						y += 20;
						msg = "Nationality: " + mrzResult.nationality;
						drawText(frame, msg.c_str(), x, y);
						y += 20;
						msg = "Issuing Country or Organization: " + mrzResult.issuer;
						drawText(frame, msg.c_str(), x, y);
						y += 20;
						msg = "Gender: " + mrzResult.gender;
						drawText(frame, msg.c_str(), x, y);
						y += 20;
						msg = "Date of Birth(YYMMDD): " + mrzResult.dateOfBirth;
						drawText(frame, msg.c_str(), x, y);
						y += 20;
						msg = "Expiration Date(YYMMDD): " + mrzResult.dateOfExpiry;

						if (captured)
						{
							captured = false;
							imshow("Captured Frame", frame);
						}
					}
				}
			}

			// Promt to change mode
			cv::putText(frame, "Press 'ESC' to quit. Press 'C' to capture.",
						cv::Point(10, 20), cv::FONT_HERSHEY_SIMPLEX,
						0.5, cv::Scalar(0, 255, 0), 2);

			imshow("MRZ Scanner", frame);
			int key = waitKey(1);
			if (key == 27 /*ESC*/)
				break;
			else if (key == char('c'))
			{
				captured = true;
			}
		}
		cvr->StopCapturing(false, true);
	}

	delete cvr, cvr = NULL;
	delete fetcher, fetcher = NULL;
	delete capturedReceiver, capturedReceiver = NULL;

	return 0;
}