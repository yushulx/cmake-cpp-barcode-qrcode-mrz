#include <stdio.h>
#include <iostream>
#include <fstream>
#include <thread>
#include "webp/mux_types.h"
#include "webp/decode.h"
#include "libwebp-1.2.1/imageio/imageio_util.h"
#include "unicode.h"

#include "DynamsoftCaptureVisionRouter.h"
#include "DynamsoftUtility.h"

using namespace std;
using namespace dynamsoft::license;
using namespace dynamsoft::cvr;
using namespace dynamsoft::dbr;
using namespace dynamsoft::utility;
using namespace dynamsoft::basic_structures;

#if !defined(_WIN32) && !defined(_WIN64)
#include <sys/time.h>

int gettime()
{
	struct timeval time;
	gettimeofday(&time, NULL);
	return (int)(time.tv_sec * 1000 * 1000 + time.tv_usec) / 1000;
}
#else
int gettime()
{
	return (int)(GetTickCount());
}
#endif

int ExUtilReadFileToWebPData(const char *const filename,
							 WebPData *const webp_data)
{
	const uint8_t *data;
	size_t size;
	if (webp_data == NULL)
		return 0;
	if (!ImgIoUtilReadFile(filename, &data, &size))
		return 0;
	webp_data->bytes = data;
	webp_data->size = size;
	return 1;
}

char *read_file_text(const char *filename)
{
	FILE *fp = fopen(filename, "r");
	size_t size;
	char *text = NULL;
	if (fp)
	{
		fseek(fp, 0, SEEK_END);
		size = ftell(fp);
	}
	else
	{
		cout << "Fail to open file" << endl;
		return NULL;
	}

	rewind(fp);
	text = (char *)calloc((size + 1), sizeof(char));
	if (text == NULL)
	{
		fputs("Memory error", stderr);
		return NULL;
	}

	char c;
	char *tmp = text;
	do
	{
		c = fgetc(fp);
		*tmp = c;
		tmp++;
	} while (c != EOF);
	fclose(fp);
	return text;
}

unsigned char *read_file_binary(const char *filename, int *out_size)
{
	FILE *fp = fopen(filename, "rb");
	size_t size;
	unsigned char *buffer = NULL;
	if (fp)
	{
		fseek(fp, 0, SEEK_END);
		size = ftell(fp);
	}
	else
	{
		cout << "Fail to open file" << endl;
		return NULL;
	}

	rewind(fp);
	buffer = (unsigned char *)malloc(sizeof(unsigned char) * size);
	if (buffer == NULL)
	{
		fputs("Memory error", stderr);
		return NULL;
	}

	size_t result = fread(buffer, 1, size, fp);
	*out_size = size;
	if (result != size)
	{
		fputs("Reading error", stderr);
		return NULL;
	}
	fclose(fp);
	return buffer;
}

int barcode_decoding(const unsigned char *buffer, int width, int height, int stride, int formats, char *license, char *config)
{
	std::thread::id thread_id = std::this_thread::get_id();

	CCaptureVisionRouter *cvr = new CCaptureVisionRouter;
	if (!cvr)
	{
		cout << "Failed to create CCaptureVisionRouter instance." << endl;
		return -1;
	}

	// Read barcodes from file stream
	int starttime = gettime();
	CImageData imageData(stride * height, buffer, width, height, stride, IPF_ARGB_8888, 0, NULL);
	CCapturedResult *result = cvr->Capture(&imageData, CPresetTemplate::PT_READ_BARCODES);
	int endtime = gettime();
	int timecost = endtime - starttime;

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

	delete cvr;
	return timecost;
}

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

int main(int argc, const char *argv[])
{
	int iRet = -1;
	char szErrorMsg[256];
	// Initialize license.
	// Request a trial from https://www.dynamsoft.com/customer/license/trialLicense/?product=dcv&package=cross-platform
	iRet = CLicenseManager::InitLicense("DLS2eyJoYW5kc2hha2VDb2RlIjoiMjAwMDAxLTE2NDk4Mjk3OTI2MzUiLCJvcmdhbml6YXRpb25JRCI6IjIwMDAwMSIsInNlc3Npb25QYXNzd29yZCI6IndTcGR6Vm05WDJrcEQ5YUoifQ==", szErrorMsg, 256);
	if (iRet != EC_OK)
	{
		cout << szErrorMsg << endl;
	}

	INIT_WARGV(argc, argv);
	const auto processor_count = std::thread::hardware_concurrency();
	printf("CPU threads: %d\n\n", processor_count);

	if (argc < 2)
	{
		printf("Usage: BarcodeReader [image-file] [optional: license-file] [optional: template-file] \n");
		return 0;
	}

	char *license = NULL;
	char *config = NULL;
	uint8_t *buffer = NULL;
	switch (argc)
	{
	case 4:
		config = read_file_text(argv[3]);
	case 3:
		license = read_file_text(argv[2]);
	}

	const W_CHAR *in_file = GET_WARGV(argv, 1);
	WebPData webp_data;
	int ok = ExUtilReadFileToWebPData((const char *)in_file, &webp_data);

	if (ok)
	{
		int width, height;
		WebPGetInfo(webp_data.bytes, webp_data.size, &width, &height);
		WebPDecBuffer output;
		GetRGBAInfo(webp_data.bytes, webp_data.size, &width, &height, &output);
		thread t1(barcode_decoding, output.u.RGBA.rgba, width, height, output.u.RGBA.stride, BF_ALL, license, config);
		t1.join();

		WebPFreeDecBuffer(&output);
	}

	WebPDataClear(&webp_data);
	if (license)
		free(license);
	if (config)
		free(config);
	if (buffer)
		free(buffer);

	return 0;
}
