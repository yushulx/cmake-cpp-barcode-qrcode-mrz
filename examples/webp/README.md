# Decode 1D/2D Barcodes from WebP Images with Dynamsoft Barcode Reader
This repository provides examples showcasing how to easily decode QR codes and other barcode formats from WebP images using Dynamsoft Barcode Reader in C/C++ and Python applications.

## Prerequisites
- Dynamsoft Barcode Reader
    - [C/C++ SDK](https://www.dynamsoft.com/barcode-reader/downloads/#desktop)
    - Python SDK
    
        ```bash
        pip install dbr opencv-python
        ```
- [libwebp-1.2.1](https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-1.2.1.tar.gz)

## License Key
Get a [30-day free trial license key](https://www.dynamsoft.com/customer/license/trialLicense/?product=dcv&package=cross-platform).

## Usage

**C/C++**
1. Create a build directory and navigate to it:
    
    ```bash
    mkdir build
    cd build
    ```
2. Configure and build the project using CMake:

    ```bash
    # Linux
    cmake ..

    # Windows
    cmake -DCMAKE_GENERATOR_PLATFORM=x64 ..

    cmake --build .
    ```

3. Run the application:

    ```bash
    ./main <webp file> 
    ```

**Python**

Run the Python script with the WebP file and license key as arguments:

```bash
python main.py <webp file> license.txt
```

## Reference
- [https://developers.google.com/speed/webp](https://developers.google.com/speed/webp)

## Blog
[How to Decode QR Code from WebP Images in C++ and Python](https://www.dynamsoft.com/codepool/webp-qr-code-recognition-cpp-python.html)
