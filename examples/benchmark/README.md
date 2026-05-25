# Barcode Benchmark: Dynamsoft DBR vs zxing-cpp

A C++ benchmarking tool that compares the decoding performance of [Dynamsoft Barcode Reader](https://www.dynamsoft.com/barcode-reader/overview/) and [zxing-cpp](https://github.com/zxing-cpp/zxing-cpp) on image files and directories.

## Features

- Processes single images or entire directories (recursive)
- Runs each library multiple times per image (default: 10) and reports avg/min/max decode time
- Tabular per-image comparison and overall summary
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
benchmark <image_or_folder> [iterations]
```

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

### Sample Output

```
=============================================================
 Barcode Benchmark: Dynamsoft DBR v11.4.20.7177 vs zxing-cpp v3.0.2
=============================================================

Found 1 image(s). Iterations per image: 3

--- Code 128.png ---
| Library    |      Avg (ms)|      Min (ms)|      Max (ms)|      Barcodes|
|------------|--------------|--------------|--------------|--------------|
| Dynamsoft  |         36.21|         17.62|         71.48|             1|
| zxing-cpp  |          9.88|          9.55|         10.26|             1|

=============================================================
 Summary (1 image(s), 3 iterations each)
=============================================================
| Library    |     Total Avg|      Total Bc|
|------------|--------------|--------------|
| Dynamsoft  |        36.21ms|             1|
| zxing-cpp  |         9.88ms|             1|
```

## Notes

- **PDF files**: Only Dynamsoft can process PDFs. zxing-cpp (via stb_image) does not support PDF input — those rows show "N/A".
- **License**: The Dynamsoft trial license key is embedded in the source. Replace it with your own license for production use.
- **Fair comparison**: zxing-cpp decodes from pre-loaded pixel data while Dynamsoft reads from file each iteration (its native API). Both timings include the full decoding pipeline.
