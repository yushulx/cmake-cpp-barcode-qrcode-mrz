/*
 * Barcode Benchmark: Dynamsoft Capture Vision vs zxing-cpp
 *
 * Usage: benchmark <image_or_folder> [iterations]
 *   image_or_folder : path to a single image file or a directory of images
 *   iterations      : number of decoding passes per image (default: 10)
 */

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <numeric>

// Prevent Windows macros from conflicting with std::min/std::max
#ifdef _WIN32
#define NOMINMAX
#endif

// stb_image for zxing-cpp image loading
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Dynamsoft Capture Vision
#include "DynamsoftCaptureVisionRouter.h"
#include "DynamsoftUtility.h"

// zxing-cpp
#include "ReadBarcode.h"
#include "BarcodeFormat.h"
#include "ImageView.h"
#include "ReaderOptions.h"
#include "Version.h"

#define ZXING_VERSION ZXING_VERSION_STR

using namespace std;
using namespace std::chrono;
using namespace dynamsoft::license;
using namespace dynamsoft::cvr;
using namespace dynamsoft::dbr;
using namespace dynamsoft::utility;
using namespace dynamsoft::basic_structures;

// ---------------------------------------------------------------------------
// Supported image extensions
// ---------------------------------------------------------------------------
static bool isSupportedImage(const string& filePath)
{
    string ext = filesystem::path(filePath).extension().string();
    transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext == ".jpg" || ext == ".jpeg" || ext == ".png" ||
           ext == ".bmp" || ext == ".tif"  || ext == ".tiff" ||
           ext == ".gif" || ext == ".pdf";
}

static vector<string> collectImages(const string& path)
{
    vector<string> files;
    if (filesystem::is_directory(path)) {
        for (const auto& entry : filesystem::recursive_directory_iterator(path)) {
            if (entry.is_regular_file() && isSupportedImage(entry.path().string()))
                files.push_back(entry.path().string());
        }
        sort(files.begin(), files.end());
    } else if (filesystem::is_regular_file(path) && isSupportedImage(path)) {
        files.push_back(path);
    }
    return files;
}

// ---------------------------------------------------------------------------
// Result structs
// ---------------------------------------------------------------------------
struct BenchResult
{
    string  libraryName;
    int     barcodeCount   = 0;
    double  avgTimeMs      = 0.0;
    double  minTimeMs      = 0.0;
    double  maxTimeMs      = 0.0;
    double  totalTimeMs    = 0.0;
};

// ---------------------------------------------------------------------------
// Dynamsoft benchmark
// ---------------------------------------------------------------------------
static BenchResult benchDynamsoft(CCaptureVisionRouter* cvr,
                                  const string& filePath,
                                  int iterations)
{
    BenchResult res;
    res.libraryName = "Dynamsoft";

    vector<double> times;
    times.reserve(iterations);
    int barcodeCount = 0;

    for (int i = 0; i < iterations; ++i)
    {
        auto t0 = high_resolution_clock::now();

        CCapturedResultArray* arr =
            cvr->CaptureMultiPages(filePath.c_str(),
                                   CPresetTemplate::PT_READ_BARCODES);

        int count = 0;
        int pageCount = arr->GetResultsCount();
        for (int p = 0; p < pageCount; ++p)
        {
            const CCapturedResult* r = arr->GetResult(p);
            if (r->GetErrorCode() != 0) continue;
            CDecodedBarcodesResult* br = r->GetDecodedBarcodesResult();
            if (br && br->GetErrorCode() == 0)
                count += br->GetItemsCount();
            if (br) br->Release();
        }
        arr->Release();

        auto t1 = high_resolution_clock::now();
        double ms = duration_cast<microseconds>(t1 - t0).count() / 1000.0;
        times.push_back(ms);
        barcodeCount = count; // last iteration count (should be stable)
    }

    res.barcodeCount = barcodeCount;
    res.totalTimeMs  = accumulate(times.begin(), times.end(), 0.0);
    res.avgTimeMs    = res.totalTimeMs / iterations;
    res.minTimeMs    = *min_element(times.begin(), times.end());
    res.maxTimeMs    = *max_element(times.begin(), times.end());
    return res;
}

// ---------------------------------------------------------------------------
// zxing-cpp benchmark
// ---------------------------------------------------------------------------
static BenchResult benchZXing(const string& filePath, int iterations)
{
    BenchResult res;
    res.libraryName = "zxing-cpp";

    // Load image once
    int width = 0, height = 0, channels = 0;
    unsigned char* buffer = stbi_load(filePath.c_str(), &width, &height, &channels, 0);
    if (!buffer) {
        cerr << "  [zxing-cpp] Failed to load image: " << filePath
             << " (" << stbi_failure_reason() << ")\n";
        return res;
    }

    // Map channel count to ZXing ImageFormat
    static const array<ZXing::ImageFormat, 5> fmtMap = {
        ZXing::ImageFormat::None,
        ZXing::ImageFormat::Lum,
        ZXing::ImageFormat::LumA,
        ZXing::ImageFormat::RGB,
        ZXing::ImageFormat::RGBA
    };
    ZXing::ImageFormat fmt = (channels >= 1 && channels <= 4)
                                 ? fmtMap[channels]
                                 : ZXing::ImageFormat::None;

    ZXing::ImageView imageView(buffer, width, height, fmt);
    ZXing::ReaderOptions options;
    options.tryHarder(true);

    vector<double> times;
    times.reserve(iterations);
    int barcodeCount = 0;

    for (int i = 0; i < iterations; ++i)
    {
        auto t0 = high_resolution_clock::now();
        auto barcodes = ZXing::ReadBarcodes(imageView, options);
        auto t1 = high_resolution_clock::now();

        double ms = duration_cast<microseconds>(t1 - t0).count() / 1000.0;
        times.push_back(ms);
        barcodeCount = static_cast<int>(barcodes.size());
    }

    stbi_image_free(buffer);

    res.barcodeCount = barcodeCount;
    res.totalTimeMs  = accumulate(times.begin(), times.end(), 0.0);
    res.avgTimeMs    = res.totalTimeMs / iterations;
    res.minTimeMs    = *min_element(times.begin(), times.end());
    res.maxTimeMs    = *max_element(times.begin(), times.end());
    return res;
}

// ---------------------------------------------------------------------------
// Print helpers
// ---------------------------------------------------------------------------
static void printSeparator(int w1, int w2, int w3, int w4, int w5)
{
    cout << "|" << string(w1, '-')
         << "|" << string(w2, '-')
         << "|" << string(w3, '-')
         << "|" << string(w4, '-')
         << "|" << string(w5, '-') << "|\n";
}

static void printHeader()
{
    cout << "|" << left << setw(12) << " Library"
         << "|" << right << setw(14) << " Avg (ms)"
         << "|" << right << setw(14) << " Min (ms)"
         << "|" << right << setw(14) << " Max (ms)"
         << "|" << right << setw(14) << " Barcodes"
         << "|\n";
    printSeparator(12, 14, 14, 14, 14);
}

static void printRow(const BenchResult& r)
{
    cout << "|" << left  << setw(12) << (" " + r.libraryName)
         << "|" << right << setw(14) << fixed << setprecision(2) << r.avgTimeMs
         << "|" << right << setw(14) << fixed << setprecision(2) << r.minTimeMs
         << "|" << right << setw(14) << fixed << setprecision(2) << r.maxTimeMs
         << "|" << right << setw(14) << r.barcodeCount
         << "|\n";
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    // Print banner
    const char* dbrVersion = CBarcodeReaderModule::GetVersion();
    cout << "=============================================================\n";
    cout << " Barcode Benchmark: Dynamsoft DBR v" << dbrVersion
         << " vs zxing-cpp v" << ZXING_VERSION << "\n";
    cout << "=============================================================\n\n";

    if (argc < 2) {
        cout << "Usage: " << (argc > 0 ? argv[0] : "benchmark")
             << " <image_or_folder> [iterations]\n"
             << "  image_or_folder : path to an image file or a directory\n"
             << "  iterations      : decoding passes per image (default: 10)\n";
        return 0;
    }

    string inputPath = argv[1];
    int iterations   = (argc >= 3) ? atoi(argv[2]) : 10;
    if (iterations < 1) iterations = 1;

    if (!filesystem::exists(inputPath)) {
        cerr << "Error: path does not exist: " << inputPath << "\n";
        return 1;
    }

    vector<string> images = collectImages(inputPath);
    if (images.empty()) {
        cerr << "Error: no supported images found in: " << inputPath << "\n";
        return 1;
    }

    cout << "Found " << images.size() << " image(s). Iterations per image: "
         << iterations << "\n\n";

    // Initialize Dynamsoft
    char szErrorMsg[256] = {0};
    int iRet = CLicenseManager::InitLicense(
        "DLS2eyJoYW5kc2hha2VDb2RlIjoiMjAwMDAxLTE2NDk4Mjk3OTI2MzUiLCJvcmdhbml6YXRpb25JRCI6IjIwMDAwMSIsInNlc3Npb25QYXNzd29yZCI6IndTcGR6Vm05WDJrcEQ5YUoifQ==",
        szErrorMsg, 256);
    if (iRet != EC_OK) {
        cerr << "Dynamsoft license error: " << szErrorMsg << "\n";
        return 1;
    }

    CCaptureVisionRouter* cvr = new CCaptureVisionRouter;
    char errorMsg[512] = {0};
    int errorCode = cvr->InitSettingsFromFile("Templates/DBR-PresetTemplates.json", errorMsg, 512);
    if (errorCode != EC_OK) {
        cerr << "Dynamsoft init error: " << errorMsg << "\n";
        delete cvr;
        return 1;
    }

    // Accumulators for overall summary
    double totalDynTime   = 0.0;
    int    totalDynBc     = 0;
    double totalZxTime    = 0.0;
    int    totalZxBc      = 0;
    int    processedCount = 0;

    for (const string& imgPath : images)
    {
        string displayName = filesystem::path(imgPath).filename().string();

        cout << "--- " << displayName << " ---\n";
        printHeader();

        // Dynamsoft
        BenchResult dynRes = benchDynamsoft(cvr, imgPath, iterations);
        printRow(dynRes);

        // zxing-cpp (skip PDFs since stb_image cannot load them)
        string ext = filesystem::path(imgPath).extension().string();
        transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".pdf") {
            cout << "|" << left << setw(12) << " zxing-cpp"
                 << "|" << right << setw(14) << "N/A (PDF)"
                 << "|" << right << setw(14) << "-"
                 << "|" << right << setw(14) << "-"
                 << "|" << right << setw(14) << "-"
                 << "|\n";
        } else {
            BenchResult zxRes = benchZXing(imgPath, iterations);
            printRow(zxRes);
            totalZxTime += zxRes.avgTimeMs;
            totalZxBc   += zxRes.barcodeCount;
        }

        totalDynTime += dynRes.avgTimeMs;
        totalDynBc   += dynRes.barcodeCount;
        ++processedCount;

        cout << "\n";
    }

    // Summary
    cout << "=============================================================\n";
    cout << " Summary (" << processedCount << " image(s), "
         << iterations << " iterations each)\n";
    cout << "=============================================================\n";
    cout << "|" << left << setw(12) << " Library"
         << "|" << right << setw(14) << " Total Avg"
         << "|" << right << setw(14) << " Total Bc"
         << "|\n";
    cout << "|" << string(12, '-') << "|" << string(14, '-')
         << "|" << string(14, '-') << "|\n";
    cout << "|" << left << setw(12) << " Dynamsoft"
         << "|" << right << setw(13) << fixed << setprecision(2)
         << totalDynTime << "ms"
         << "|" << right << setw(14) << totalDynBc << "|\n";
    cout << "|" << left << setw(12) << " zxing-cpp"
         << "|" << right << setw(13) << fixed << setprecision(2)
         << totalZxTime << "ms"
         << "|" << right << setw(14) << totalZxBc << "|\n";
    cout << "\n";

    delete cvr;
    return 0;
}
