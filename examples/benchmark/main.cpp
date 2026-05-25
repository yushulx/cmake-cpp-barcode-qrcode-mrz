/*
 * Barcode Benchmark: Dynamsoft Capture Vision vs zxing-cpp
 *
 * Usage: benchmark [--threads N] <image_or_folder> [iterations]
 *   image_or_folder : path to a single image file or a directory of images
 *   iterations      : number of decoding passes per image (default: 10)
 *   --threads N     : run multi-threaded throughput benchmark with N threads
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
#include <thread>
#include <mutex>
#include <atomic>

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
// Multi-threading types
// ---------------------------------------------------------------------------
struct MultiThreadResult
{
    string  libraryName;
    int     totalBarcodes  = 0;
    double  wallTimeSec    = 0.0;
    double  throughput     = 0.0;
    int     totalDecodes   = 0;
};

// ---------------------------------------------------------------------------
// Dynamsoft multi-thread benchmark
// ---------------------------------------------------------------------------
static MultiThreadResult benchDynamsoftMulti(CCaptureVisionRouter* cvr,
                                              const vector<string>& images,
                                              int iterations, int numThreads)
{
    (void)cvr; // unused in this implementation (each thread creates its own router)
    MultiThreadResult res;
    res.libraryName = "Dynamsoft";

    auto t0 = high_resolution_clock::now();
    atomic<int> totalBarcodes{0};
    vector<thread> workers;

    for (int t = 0; t < numThreads; ++t)
    {
        workers.emplace_back([&, t]() {
            CCaptureVisionRouter* localCvr = new CCaptureVisionRouter;
            char err[512] = {0};
            localCvr->InitSettingsFromFile("Templates/DBR-PresetTemplates.json", err, 512);

            int localBc = 0;
            for (size_t i = t; i < images.size(); i += numThreads)
            {
                for (int it = 0; it < iterations; ++it)
                {
                    CCapturedResultArray* arr = localCvr->CaptureMultiPages(
                        images[i].c_str(), CPresetTemplate::PT_READ_BARCODES);

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
                    if (it == 0) localBc = count;
                }
                totalBarcodes.fetch_add(localBc);
            }
            delete localCvr;
        });
    }
    for (auto& w : workers) w.join();

    auto t1 = high_resolution_clock::now();
    res.wallTimeSec = duration_cast<microseconds>(t1 - t0).count() / 1e6;
    res.totalBarcodes = totalBarcodes.load();
    res.totalDecodes = static_cast<int>(images.size()) * iterations;
    res.throughput = res.totalDecodes / res.wallTimeSec;
    return res;
}

// ---------------------------------------------------------------------------
// zxing-cpp multi-thread benchmark
// ---------------------------------------------------------------------------
static MultiThreadResult benchZXingMulti(const vector<string>& images,
                                          int iterations, int numThreads)
{
    MultiThreadResult res;
    res.libraryName = "zxing-cpp";

    // Filter out PDFs (stb_image cannot load them)
    vector<string> validImages;
    for (const auto& img : images) {
        string ext = filesystem::path(img).extension().string();
        transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext != ".pdf")
            validImages.push_back(img);
    }
    if (validImages.empty()) return res;

    ZXing::ReaderOptions options;
    options.tryHarder(true);
    static const array<ZXing::ImageFormat, 5> fmtMap = {
        ZXing::ImageFormat::None, ZXing::ImageFormat::Lum,
        ZXing::ImageFormat::LumA, ZXing::ImageFormat::RGB,
        ZXing::ImageFormat::RGBA
    };

    auto t0 = high_resolution_clock::now();
    atomic<int> totalBarcodes{0};
    vector<thread> workers;

    for (int t = 0; t < numThreads; ++t)
    {
        workers.emplace_back([&, t]() {
            for (size_t i = t; i < validImages.size(); i += numThreads)
            {
                int width = 0, height = 0, channels = 0;
                unsigned char* buf = stbi_load(validImages[i].c_str(), &width, &height, &channels, 0);
                if (!buf) continue;

                ZXing::ImageFormat fmt = (channels >= 1 && channels <= 4)
                                             ? fmtMap[channels]
                                             : ZXing::ImageFormat::None;
                ZXing::ImageView iv(buf, width, height, fmt);

                int localBc = 0;
                for (int it = 0; it < iterations; ++it)
                {
                    auto bc = ZXing::ReadBarcodes(iv, options);
                    if (it == 0) localBc = static_cast<int>(bc.size());
                }
                totalBarcodes.fetch_add(localBc);
                stbi_image_free(buf);
            }
        });
    }
    for (auto& w : workers) w.join();

    auto t1 = high_resolution_clock::now();
    res.wallTimeSec = duration_cast<microseconds>(t1 - t0).count() / 1e6;
    res.totalBarcodes = totalBarcodes.load();
    res.totalDecodes = static_cast<int>(validImages.size()) * iterations;
    res.throughput = res.totalDecodes / res.wallTimeSec;
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
// Multi-thread print helpers
// ---------------------------------------------------------------------------
static void printMultiHeader(int numThreads)
{
    cout << "--- Multi-Threaded Benchmark (" << numThreads << " threads) ---\n";
    cout << "|" << left << setw(12) << " Library"
         << "|" << right << setw(14) << " Wall Time (s)"
         << "|" << right << setw(14) << " Decodes/s"
         << "|" << right << setw(14) << " Total Decodes"
         << "|" << right << setw(14) << " Barcodes"
         << "|\n";
    cout << "|" << string(12, '-') << "|" << string(14, '-')
         << "|" << string(14, '-') << "|" << string(14, '-')
         << "|" << string(14, '-') << "|\n";
}

static void printMultiRow(const MultiThreadResult& r)
{
    cout << "|" << left  << setw(12) << (" " + r.libraryName)
         << "|" << right << setw(14) << fixed << setprecision(3) << r.wallTimeSec
         << "|" << right << setw(14) << fixed << setprecision(1) << r.throughput
         << "|" << right << setw(14) << r.totalDecodes
         << "|" << right << setw(14) << r.totalBarcodes
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

    // --- Parse arguments ---
    int numThreads = 0;
    int iterations = 10;
    string inputPath;

    vector<string> posArgs;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--threads") == 0) {
            if (i + 1 < argc) {
                numThreads = atoi(argv[++i]);
                if (numThreads < 1) numThreads = 1;
            } else {
                cerr << "Error: --threads requires a number argument\n";
                return 1;
            }
        } else {
            posArgs.push_back(argv[i]);
        }
    }

    if (posArgs.empty()) {
        cout << "Usage: " << (argc > 0 ? argv[0] : "benchmark")
             << " [--threads N] <image_or_folder> [iterations]\n"
             << "  image_or_folder : path to an image file or a directory\n"
             << "  iterations      : decoding passes per image (default: 10)\n"
             << "  --threads N     : run multi-threaded throughput benchmark with N threads\n";
        return 0;
    }

    inputPath = posArgs[0];
    if (posArgs.size() >= 2) {
        iterations = atoi(posArgs[1].c_str());
        if (iterations < 1) iterations = 1;
    }

    if (!filesystem::exists(inputPath)) {
        cerr << "Error: path does not exist: " << inputPath << "\n";
        return 1;
    }

    vector<string> images = collectImages(inputPath);
    if (images.empty()) {
        cerr << "Error: no supported images found in: " << inputPath << "\n";
        return 1;
    }

    cout << "Found " << images.size() << " image(s). "
         << "Iterations per image: " << iterations;
    if (numThreads > 0)
        cout << ", Threads: " << numThreads;
    cout << "\n\n";

    // --- Initialize Dynamsoft ---
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

    // ================================================================
    // Single-thread benchmark (per-image detail)
    // ================================================================
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

    // Single-thread summary
    cout << "=============================================================\n";
    cout << " Single-Thread Summary (" << processedCount << " image(s), "
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

    // ================================================================
    // Multi-thread benchmark (optional)
    // ================================================================
    if (numThreads > 0)
    {
        cout << "=============================================================\n";
        printMultiHeader(numThreads);

        MultiThreadResult dynMulti = benchDynamsoftMulti(cvr, images, iterations, numThreads);
        printMultiRow(dynMulti);

        MultiThreadResult zxMulti = benchZXingMulti(images, iterations, numThreads);
        printMultiRow(zxMulti);

        // Speedup vs single-thread
        if (totalDynTime > 0 && dynMulti.wallTimeSec > 0) {
            double dynSeqTotal = totalDynTime * iterations / 1000.0;
            double dynSpeedup  = dynSeqTotal / dynMulti.wallTimeSec;
            double zxSeqTime = totalZxTime / 1000.0;
            if (zxSeqTime > 0 && zxMulti.wallTimeSec > 0) {
                double zxSeqTotal = zxSeqTime * iterations;
                double zxSpeedup  = zxSeqTotal / zxMulti.wallTimeSec;
                cout << fixed << setprecision(2);
                cout << "Speedup:  Dynamsoft " << dynSpeedup << "x,  zxing-cpp "
                     << zxSpeedup << "x" << std::endl;
            }
        }
        cout << "\n";
    }

    delete cvr;
    return 0;
}
