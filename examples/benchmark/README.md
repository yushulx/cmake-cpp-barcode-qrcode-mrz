# Barcode Benchmark: Dynamsoft DBR vs zxing-cpp

A C++ benchmarking tool that compares the decoding performance of [Dynamsoft Barcode Reader](https://www.dynamsoft.com/barcode-reader/overview/) and [zxing-cpp](https://github.com/zxing-cpp/zxing-cpp) on image files and directories.

## Features

- Processes single images or entire directories (recursive)
- Single-thread benchmark: runs each library multiple times per image and reports avg/min/max decode time
- Multi-thread benchmark (optional): processes all images concurrently across N threads, reports throughput and wall-clock time
- Tabular per-image single-thread comparison + overall summary
- Supports common image formats: JPG, PNG, BMP, TIFF, GIF, PDF (Dynamsoft only)
- Cross-platform CMake build (Windows, Linux, macOS)

## Prerequisites

- **CMake 3.8+**
- **C++17 compiler** (MSVC, GCC, Clang)
- **Dynamsoft Capture Vision SDK** — available in `../../dcv/` (relative to this project). Get a trial license from [Dynamsoft](https://www.dynamsoft.com/customer/license/trialLicense/?product=dcv&package=cross-platform).
- **zxing-cpp** prebuilt library — the static lib (`ZXing.lib` / `libZXing.a`) must be placed in `lib/zxing-cpp/`. Headers are bundled locally in `zxing-cpp/include/`.

## Project Structure

```
benchmark/
├── CMakeLists.txt           # CMake build configuration
├── main.cpp                 # Benchmark source code
├── lib/
│   └── zxing-cpp/
│       └── ZXing.lib        # Prebuilt zxing-cpp static library
├── zxing-cpp/
│   └── include/             # zxing-cpp header files (local copy)
├── stb/
│   ├── stb_image.h          # Image loading for zxing-cpp (header-only)
│   └── stb_image_write.h
├── build/                   # Build output directory
└── README.md
```

## Build

```bash
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

On Linux/macOS:

```bash
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

After a successful build the executable is at `build/Release/benchmark.exe` (Windows) or `build/benchmark` (Unix). The post-build step automatically copies Dynamsoft DLLs and resource files (Templates, Models) next to the executable.

## Usage

```bash
benchmark [--threads N] <image_or_folder> [iterations]
```

- `--threads N` — (optional) run multi-threaded throughput benchmark with N threads after single-thread comparison.
- `image_or_folder` — path to a single image file or a directory (scanned recursively).
- `iterations` — number of decoding passes per image (default: 10). Higher values give more stable timing averages.

### Examples

Benchmark a single image with 10 iterations:

```bash
benchmark "C:\images\QRCode.png"
```

Benchmark a folder with 5 iterations:

```bash
benchmark "C:\images" 5
```

Benchmark a folder with 5 iterations and 4 threads (multi-thread throughput):

```bash
benchmark --threads 4 "C:\images" 5
```

### Sample Output (multi-threaded)

With `--threads 4`, the multi-thread benchmark section is appended after the single-thread summary:

```
=============================================================
 Single-Thread Summary (44 image(s), 3 iterations each)
=============================================================
| Library    |     Total Avg|      Total Bc|
|------------|--------------|--------------|
| Dynamsoft  |      3326.84ms|           353|
| zxing-cpp  |      1960.86ms|           232|

=============================================================
--- Multi-Threaded Benchmark (4 threads) ---
| Library    | Wall Time (s)|     Decodes/s| Total Decodes|      Barcodes|
|------------|--------------|--------------|--------------|--------------|
| Dynamsoft  |         3.726|          35.4|           132|           353|
| zxing-cpp  |         2.368|          54.5|           129|           232|
Speedup:  Dynamsoft 2.93x,  zxing-cpp 2.53x
```

## Notes

- **PDF files**: Only Dynamsoft can process PDFs. zxing-cpp (via stb_image) does not support PDF input — those rows show "N/A".
- **License**: The Dynamsoft trial license key is embedded in the source. Replace it with your own license for production use.
- **Fair comparison**: zxing-cpp decodes from pre-loaded pixel data while Dynamsoft reads from file each iteration (its native API). Both timings include the full decoding pipeline.
- **Multi-threading**: Each thread creates its own Dynamsoft `CCaptureVisionRouter` instance since the SDK is not thread-safe for concurrent decode calls on a single router. zxing-cpp's `ReadBarcodes` is thread-safe and each thread loads its own image data independently.
