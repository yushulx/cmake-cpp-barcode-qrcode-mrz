The sample demonstrates how to use Dynamsoft Capture Vision modules - **Dynamsoft Barcode Reader** and **Dynamsoft Label Recognizer** - to read barcodes and the surrounding text from a camera stream using C++.

## Usage
1. Set the license key in `main.cpp`.
    
    ```cpp
    CLicenseManager::InitLicense("LICENSE-KEY");
    ```

2. Build the project:

    ```bash
    mkdir build
    cd build
    cmake ..
    cmake --build .
    ```

    ![scan barcode and OCR text in C++](https://www.dynamsoft.com/codepool/img/2024/05/cpp-barcode-ocr-text.jpg)