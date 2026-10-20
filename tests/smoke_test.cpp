// Cross-platform smoke test for the Dynamsoft Capture Vision SDK bundled in
// this repository. It exercises the three code paths the tutorials rely on:
//
//   barcode  - decode a single image with the built-in ReadBarcodes_Default template
//   pages    - decode every page of a multi-page PDF with CaptureMultiPages
//   mrz      - read and parse a passport MRZ with ReadPassportAndId
//
// The program exits with 0 when every expectation holds and 1 otherwise, so it
// can be wired into CTest and run on Windows, Linux and macOS in CI.

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

#include "DynamsoftBarcodeReader.h"
#include "DynamsoftCaptureVisionRouter.h"
#include "DynamsoftLicense.h"

using namespace dynamsoft::basic_structures;
using namespace dynamsoft::cvr;
using namespace dynamsoft::dbr;
using namespace dynamsoft::dcp;
using namespace dynamsoft::license;

namespace
{
	const char *kDefaultBarcodeTemplate = CPresetTemplate::PT_READ_BARCODES;
	const char *kMrzTemplate = "ReadPassportAndId";

	struct Options
	{
		std::string mode = "barcode";
		std::string file;
		std::string licenseFile;
		int minCount = 1;
	};

	void PrintUsage(const char *exe)
	{
		std::cout << "Usage: " << exe << " [options] <image-or-pdf>\n"
				  << "  --mode barcode|pages|mrz  Test mode (default: barcode)\n"
				  << "  --min <n>                 Minimum expected result count (default: 1)\n"
				  << "  --license-file <path>     File holding the license key\n"
				  << "                            (overridden by DYNAMSOFT_LICENSE_KEY)\n";
	}

	bool ParseInt(const std::string &text, int &value)
	{
		try
		{
			size_t consumed = 0;
			int parsed = std::stoi(text, &consumed);
			if (consumed != text.size())
				return false;
			value = parsed;
			return true;
		}
		catch (const std::exception &)
		{
			return false;
		}
	}

	std::string Trim(const std::string &value)
	{
		const char *whitespace = " \t\r\n\"";
		size_t first = value.find_first_not_of(whitespace);
		if (first == std::string::npos)
			return std::string();
		size_t last = value.find_last_not_of(whitespace);
		return value.substr(first, last - first + 1);
	}

	bool LoadLicenseKey(const Options &options, std::string &key)
	{
		const char *fromEnv = std::getenv("DYNAMSOFT_LICENSE_KEY");
		if (fromEnv != NULL && fromEnv[0] != '\0')
		{
			key = Trim(fromEnv);
			return !key.empty();
		}

		if (options.licenseFile.empty())
			return false;

		std::ifstream input(options.licenseFile.c_str());
		if (!input.good())
			return false;

		std::string firstLine;
		std::getline(input, firstLine);
		key = Trim(firstLine);
		return !key.empty();
	}

	int Fail(const std::string &message)
	{
		std::cout << "[FAIL] " << message << std::endl;
		return 1;
	}

	// Counts the barcode items carried by a single captured result. Pages that
	// hold no barcode are not an error: the caller decides the threshold.
	int CountBarcodes(const CCapturedResult *result)
	{
		if (result == NULL)
			return 0;

		int errorCode = result->GetErrorCode();
		if (errorCode != EC_OK && errorCode != EC_UNSUPPORTED_JSON_KEY_WARNING)
		{
			std::cout << "[WARN] page " << (result->GetOriginalImageTag() != NULL
											   ? result->GetOriginalImageTag()->GetImageId()
											   : -1)
					  << ": " << result->GetErrorString() << std::endl;
			return 0;
		}

		CDecodedBarcodesResult *barcodes = result->GetDecodedBarcodesResult();
		if (barcodes == NULL)
			return 0;

		int count = 0;
		if (barcodes->GetErrorCode() == EC_OK)
			count = barcodes->GetItemsCount();

		barcodes->Release();
		return count;
	}

	int RunBarcodeTest(CCaptureVisionRouter *router, const Options &options)
	{
		CCapturedResult *result = router->Capture(options.file.c_str(), kDefaultBarcodeTemplate);
		if (result == NULL)
			return Fail("Capture() returned null for " + options.file);

		int count = CountBarcodes(result);
		int errorCode = result->GetErrorCode();
		std::string errorString = result->GetErrorString() != NULL ? result->GetErrorString() : "";
		result->Release();

		if (errorCode != EC_OK && errorCode != EC_UNSUPPORTED_JSON_KEY_WARNING)
			return Fail("Capture() failed with " + std::to_string(errorCode) + ": " + errorString);

		if (count < options.minCount)
			return Fail("expected at least " + std::to_string(options.minCount) + " barcode(s), decoded " + std::to_string(count));

		std::cout << "[PASS] mode=barcode file=" << options.file << " barcodes=" << count
				  << " (min " << options.minCount << ")" << std::endl;
		return 0;
	}

	int RunMultiPageTest(CCaptureVisionRouter *router, const Options &options)
	{
		CCapturedResultArray *results = router->CaptureMultiPages(options.file.c_str(), kDefaultBarcodeTemplate);
		if (results == NULL)
			return Fail("CaptureMultiPages() returned null for " + options.file);

		int pageCount = results->GetResultsCount();
		int total = 0;
		for (int i = 0; i < pageCount; i++)
			total += CountBarcodes(results->GetResult(i));
		results->Release();

		if (pageCount < 2)
			return Fail("expected a multi-page file, read " + std::to_string(pageCount) + " page(s)");

		if (total < options.minCount)
			return Fail("expected at least " + std::to_string(options.minCount) + " barcode(s) across pages, decoded " + std::to_string(total));

		std::cout << "[PASS] mode=pages file=" << options.file << " pages=" << pageCount
				  << " barcodes=" << total << " (min " << options.minCount << ")" << std::endl;
		return 0;
	}

	int RunMrzTest(CCaptureVisionRouter *router, const Options &options)
	{
		CCapturedResult *result = router->Capture(options.file.c_str(), kMrzTemplate);
		if (result == NULL)
			return Fail("Capture() returned null for " + options.file);

		int errorCode = result->GetErrorCode();
		std::string errorString = result->GetErrorString() != NULL ? result->GetErrorString() : "";

		int parsedCount = 0;
		CParsedResult *parsed = result->GetParsedResult();
		if (parsed != NULL)
		{
			if (parsed->GetErrorCode() == EC_OK)
				parsedCount = parsed->GetItemsCount();
			parsed->Release();
		}
		result->Release();

		if (errorCode != EC_OK && errorCode != EC_UNSUPPORTED_JSON_KEY_WARNING)
			return Fail("Capture() failed with " + std::to_string(errorCode) + ": " + errorString);

		if (parsedCount < options.minCount)
			return Fail("expected at least " + std::to_string(options.minCount) + " MRZ zone(s), parsed " + std::to_string(parsedCount));

		std::cout << "[PASS] mode=mrz file=" << options.file << " zones=" << parsedCount
				  << " (min " << options.minCount << ")" << std::endl;
		return 0;
	}
} // namespace

int main(int argc, char *argv[])
{
	Options options;
	for (int i = 1; i < argc; i++)
	{
		std::string arg = argv[i];
		if (arg == "--mode" && i + 1 < argc)
			options.mode = argv[++i];
		else if (arg == "--min" && i + 1 < argc)
		{
			if (!ParseInt(argv[++i], options.minCount))
				return Fail("invalid --min value");
		}
		else if (arg == "--license-file" && i + 1 < argc)
			options.licenseFile = argv[++i];
		else if (arg == "--help" || arg == "-h")
		{
			PrintUsage(argv[0]);
			return 0;
		}
		else if (arg.rfind("--", 0) == 0)
			return Fail("unknown option " + arg);
		else if (options.file.empty())
			options.file = arg;
		else
			return Fail("only one input file is supported");
	}

	if (options.file.empty())
	{
		PrintUsage(argv[0]);
		return 2;
	}

	if (options.mode != "barcode" && options.mode != "pages" && options.mode != "mrz")
		return Fail("unknown mode " + options.mode);

	std::cout << "Dynamsoft Capture Vision " << CCaptureVisionRouterModule::GetVersion()
			  << " / Barcode Reader " << CBarcodeReaderModule::GetVersion() << std::endl;

	std::string licenseKey;
	if (!LoadLicenseKey(options, licenseKey))
		return Fail("no license key: set DYNAMSOFT_LICENSE_KEY or pass --license-file");

	char errorMessage[512] = {0};
	int licenseStatus = CLicenseManager::InitLicense(licenseKey.c_str(), errorMessage, 512);
	if (licenseStatus != EC_OK && licenseStatus != EC_LICENSE_WARNING)
		return Fail("InitLicense() failed with " + std::to_string(licenseStatus) + ": " + errorMessage);
	if (licenseStatus == EC_LICENSE_WARNING)
		std::cout << "[WARN] " << errorMessage << std::endl;

	CCaptureVisionRouter router;
	int exitCode = 0;
	if (options.mode == "barcode")
		exitCode = RunBarcodeTest(&router, options);
	else if (options.mode == "pages")
		exitCode = RunMultiPageTest(&router, options);
	else
		exitCode = RunMrzTest(&router, options);

	return exitCode;
}
