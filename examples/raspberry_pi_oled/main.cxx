// A headless C++ barcode and QR code reader for Raspberry Pi.
//
// The program captures frames from a USB camera through V4L2 (no GUI,
// no OpenCV), decodes barcodes with the Dynamsoft Capture Vision SDK and
// shows the results on an SSD1306 OLED display and the console.
//
// Usage:
//   ./barcode_reader [--device /dev/video0] [--width 640] [--height 480]
//                    [--duration 120] [--no-oled]
//
// The loop never runs forever: it stops when 'q' is pressed, when Ctrl+C
// is received (SIGINT) or when the run duration (default 120 seconds,
// 0 disables the timer) is reached.

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

#include "DynamsoftCaptureVisionRouter.h"
#include "DynamsoftLicense.h"

#include "ssd1306_i2c.h"
#include "v4l2_capture.h"

using namespace std;
using namespace dynamsoft::license;
using namespace dynamsoft::cvr;
using namespace dynamsoft::dbr;
using namespace dynamsoft::basic_structures;

// This is a free public trial license based on the organization ID, which
// requires a network connection. For a private license key, register at
// https://www.dynamsoft.com/customer/license/trialLicense/?product=dcv&package=cross-platform
#define LICENSE_KEY "DLS2eyJoYW5kc2hha2VDb2RlIjoiMjAwMDAxLTE2NDk4Mjk3OTI2MzUiLCJvcmdhbml6YXRpb25JRCI6IjIwMDAwMSIsInNlc3Npb25QYXNzd29yZCI6IndTcGR6Vm05WDJrcEQ5YUoifQ=="

#define OLED_LINE_WIDTH 21 // 128 px / 6 px per character, 4 lines of 8 px

static std::atomic<bool> gStopped(false);
static bool gOledEnabled = false;

static void handleSignal(int)
{
	gStopped = true;
}

// Non-blocking check whether 'q' was pressed in the terminal.
static bool quitRequested()
{
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(STDIN_FILENO, &fds);
	struct timeval tv = {0, 0};
	if (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0)
	{
		char ch;
		if (read(STDIN_FILENO, &ch, 1) == 1 && (ch == 'q' || ch == 'Q'))
		{
			return true;
		}
	}
	return false;
}

// Wraps a long barcode text into lines that fit the 128x32 OLED (21x3 chars).
static string wrapOLEDText(const string &text, int maxLines)
{
	vector<string> lines;
	string line;
	for (char c : text)
	{
		if (c == '\n')
		{
			if (!line.empty())
			{
				lines.push_back(line);
				line.clear();
			}
		}
		else if (c != '\r')
		{
			line += c;
			if ((int)line.length() >= OLED_LINE_WIDTH)
			{
				lines.push_back(line);
				line.clear();
			}
		}
	}
	if (!line.empty())
	{
		lines.push_back(line);
	}
	string result;
	for (size_t i = 0; i < lines.size() && i < (size_t)maxLines; ++i)
	{
		if (i > 0)
		{
			result += '\n';
		}
		result += lines[i];
	}
	if (lines.size() > (size_t)maxLines)
	{
		result += "...";
	}
	return result;
}

static void drawOLED(const string &content)
{
	if (!gOledEnabled)
	{
		return;
	}
	ssd1306_clearDisplay();
	ssd1306_drawText(0, 0, (char *)content.c_str());
	ssd1306_display();
}

int main(int argc, char *argv[])
{
	// Parse command line arguments.
	string device = "/dev/video0";
	int width = 640;
	int height = 480;
	int durationSeconds = 120;
	bool useOled = true;
	for (int i = 1; i < argc; ++i)
	{
		string arg = argv[i];
		if (arg == "--no-oled")
		{
			useOled = false;
		}
		else if (arg == "--help")
		{
			cout << "Usage: " << argv[0]
				 << " [--device /dev/video0] [--width 640] [--height 480]"
				 << " [--duration 120] [--no-oled]" << endl;
			return 0;
		}
		else if (i + 1 < argc)
		{
			string value = argv[i + 1];
			if (arg == "--device")
			{
				device = value;
				++i;
			}
			else if (arg == "--width")
			{
				width = atoi(value.c_str());
				++i;
			}
			else if (arg == "--height")
			{
				height = atoi(value.c_str());
				++i;
			}
			else if (arg == "--duration")
			{
				durationSeconds = atoi(value.c_str());
				++i;
			}
		}
	}

	signal(SIGINT, handleSignal);
	signal(SIGTERM, handleSignal);

	// Initialize the license.
	char errorMsg[512] = {0};
	if (CLicenseManager::InitLicense(LICENSE_KEY, errorMsg, 512) != EC_OK)
	{
		cout << "Failed to init license: " << errorMsg << endl;
	}

	// Initialize the OLED display (optional). The display is probed on the I2C
	// bus; if there is no OLED attached, the program runs without it.
	if (useOled)
	{
		if (ssd1306_begin(SSD1306_SWITCHCAPVCC, SSD1306_I2C_ADDRESS) == 1)
		{
			gOledEnabled = true;
			drawOLED("Dynamsoft\nBarcode Reader");
		}
		else
		{
			cout << "OLED display not found (pass --no-oled to silence this check); "
					"continuing without it." << endl;
		}
	}
	else
	{
		cout << "OLED display disabled by --no-oled." << endl;
	}

	// Open the camera.
	V4L2Capture camera;
	if (!camera.open(device.c_str(), width, height))
	{
		cout << "ERROR: cannot open camera " << device << endl;
		drawOLED("Camera\nError");
		return 1;
	}
	width = camera.getWidth();
	height = camera.getHeight();

	cout << "Dynamsoft Barcode Reader (headless)" << endl;
	cout << "Camera: " << device << " (" << width << "x" << height << ")" << endl;
	cout << "OLED: " << (gOledEnabled ? "enabled (128x32, /dev/i2c-1)" : "not available") << endl;
	if (durationSeconds > 0)
	{
		cout << "Auto-stop: " << durationSeconds << " seconds" << endl;
	}
	else
	{
		cout << "Auto-stop: disabled" << endl;
	}
	cout << "Press 'q' or Ctrl+C to quit." << endl;

	// Create the Capture Vision Router.
	CCaptureVisionRouter *cvr = new CCaptureVisionRouter;

	vector<unsigned char> frameRGB;
	CFileImageTag tag(nullptr, 0, 0);
	unsigned long long frames = 0;
	unsigned long long framesWithResults = 0;
	string lastShownText;
	auto startTime = chrono::steady_clock::now();

	drawOLED("Scanning...\nPress q to quit");

	while (!gStopped)
	{
		if (durationSeconds > 0 &&
			chrono::duration_cast<chrono::seconds>(chrono::steady_clock::now() - startTime).count() >= durationSeconds)
		{
			break;
		}
		if (quitRequested())
		{
			break;
		}

		if (!camera.readFrame(frameRGB))
		{
			continue;
		}
		++frames;

		tag.SetImageId((int)frames);
		CImageData imageData(frameRGB.size(),
							 frameRGB.data(),
							 width,
							 height,
							 width * 3,
							 IPF_RGB_888,
							 0,
							 &tag);

		CCapturedResult *result = cvr->Capture(&imageData, CPresetTemplate::PT_READ_BARCODES);
		if (result == NULL)
		{
			continue;
		}

		CDecodedBarcodesResult *barcodeResult = result->GetDecodedBarcodesResult();
		if (barcodeResult != NULL)
		{
			int count = barcodeResult->GetItemsCount();
			if (count > 0)
			{
				++framesWithResults;
				const CBarcodeResultItem *item = barcodeResult->GetItem(0);
				if (item != NULL)
				{
					const char *format = item->GetFormatString();
					const char *text = item->GetText();
					if (format == NULL)
					{
						format = "unknown";
					}
					if (text == NULL)
					{
						text = "";
					}
					if (lastShownText != text)
					{
						lastShownText = text;
						cout << "Frame " << frames << ": " << format << " -> " << text << endl;
						drawOLED("Found: " + string(format) + "\n" + wrapOLEDText(text, 3));
					}
				}
			}
			barcodeResult->Release();
		}
		result->Release();
	}

	// Print the summary.
	long long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - startTime).count();
	cout << "Processed " << frames << " frames in " << elapsed / 1000.0 << "s ("
		 << (elapsed > 0 ? frames * 1000.0 / elapsed : 0.0) << " fps)"
		 << ", barcode found in " << framesWithResults << " frames" << endl;

	delete cvr;

	drawOLED("Reader stopped");
	usleep(1000 * 1000);
	return 0;
}
