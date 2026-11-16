# Building 1D/2D Barcode Reader with C++ and CMake
This repository contains examples demonstrating how to utilize the **Dynamsoft Barcode Reader SDK** to build barcode and QR code detection applications with C++ and CMake on **Windows**, **Linux**, **macOS**, and **Raspberry Pi OS**.

The bundled SDK is **Dynamsoft Capture Vision 3.6.50** (Dynamsoft Barcode Reader 11.6.50) under [`dcv/`](./dcv). `dcv/lib/mac` ships universal binaries, and `dcv/lib/linux` covers both `x64` and `arm64`.

## Prerequisites
- Obtain a [30-day free trial license](https://www.dynamsoft.com/customer/license/trialLicense/?product=dcv&package=cross-platform) for Dynamsoft Barcode Reader.

    ```cpp
    CLicenseManager::InitLicense("LICENSE-KEY", szErrorMsg, 256);
    ```

- OpenCV Installation
    1. Download [OpenCV](https://opencv.org/releases/)
    2. Configure the environment variable `OpenCV_DIR` to the path of the OpenCV installation directory.
    3. Add the following lines to the `CMakeLists.txt` file:
    
        ```cmake
        # Find OpenCV, you may need to set OpenCV_DIR variable
        # to the absolute path to the directory containing OpenCVConfig.cmake file
        # via the command line or GUI
        find_package(OpenCV REQUIRED)
        ```

## Supported Platforms
- Windows x64 (MSVC, MinGW)
- Linux x64/ARM64
- macOS arm64 + x64 (universal binaries)

## Tests

[`tests/`](./tests) is a standalone CMake project that loads the bundled SDK and checks the code paths the tutorials rely on. It runs without a camera, OpenCV, or Qt.

```bash
cmake -S tests -B build/tests -DCMAKE_BUILD_TYPE=Release
cmake --build build/tests --config Release
ctest --test-dir build/tests -C Release --output-on-failure
```

| Test | Input | Expectation |
| --- | --- | --- |
| `barcode_image` | `images/AllSupportedBarcodeTypes.png` | at least 15 of the 18 barcodes decoded |
| `barcode_multipage_pdf` | `examples/command-line/multi_page.pdf` | 4 pages read, at least 4 barcodes |
| `mrz_passport` | `images/passport-sample.jpg` | at least 1 MRZ zone parsed by `ReadPassportAndId` |

### Setting the Dynamsoft license key

[`license-key.txt`](./license-key.txt) ships with a **public evaluation key that Dynamsoft issues for one day** — enough to run the samples and CI as-is, and it is replaced whenever it lapses. For anything longer, supply your own key; it takes precedence in this order:

1. the `DYNAMSOFT_LICENSE_KEY` environment variable;
2. the `license-key.txt` file in the repository root.

```bash
# Git Bash / Linux / macOS
export DYNAMSOFT_LICENSE_KEY="DLS2..."

# or replace the file, which is what CI and the samples read by default
echo "DLS2..." > license-key.txt
```

On Windows use **System Properties → Environment Variables**, or set it for the current session only:

```
set DYNAMSOFT_LICENSE_KEY=DLS2...
```

Apply for a free 30-day trial at the [Dynamsoft customer portal](https://www.dynamsoft.com/customer/license/trialLicense/?product=dcv&package=cross-platform) and pick **Barcode Reader / DCV**; choose the *Linux x64* platform as well if you intend to run the Docker benchmark. A `24-hour Temporary license expired` error means the bundled evaluation key has lapsed — set your own as above.

The GitHub Actions workflow [`.github/workflows/cmake.yml`](./.github/workflows/cmake.yml) builds `examples/command-line`, runs it against `images/AllSupportedBarcodeTypes.png`, and executes the three tests above on Windows, Linux, and macOS.

## How to Build a CMake Project

**Windows**

1. Create a **build** folder:
    
    ```bash
    mkdir build
    cd build
    ```

2. Configure and build the project:
    
    ```bash
    # x86
    cmake -DCMAKE_GENERATOR_PLATFORM=x86 ..

    # x64
    cmake -DCMAKE_GENERATOR_PLATFORM=x64 ..
    
    cmake --build . --config release
    ```

    For `MinGW`:
    
    ```bash
    cmake -G "MinGW Makefiles" ..
    ```

**Linux and Raspberry Pi OS**

1. Install **CMake**:
    
    ```bash
    sudo apt-get install cmake
    ```

2. Create a **build** folder:
    
    ```bash
    mkdir build
    cd build
    ```

3. Configure and build the project:
    ```bash
    cmake ..
    cmake --build . --config release 
    ```

**macOS**
 
1. Install **CMake**:
    
    ```bash
    brew install cmake
    ```

2. Create a **build** folder:

    ```bash
    mkdir build
    cd build
    ```

3. Configure and build the project:

    ```bash
    cmake ..
    cmake --build . --config release 
    ```

## Examples
- [Command Line](./examples/command-line)
- [Barcode Image File](./examples/opencv_file)
- [Camera Scanning](./examples/opencv_camera)
- [WebP](./examples/webp/)

