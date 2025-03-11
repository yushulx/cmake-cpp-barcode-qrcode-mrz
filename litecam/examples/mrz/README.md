# C++ MRZ Scanner
A C++ application for scanning and processing **Machine Readable Zones (MRZ)** from identification documents such as **passports**, **ID cards**, and **visas**. The application can extract standardized information such as document type, issuing country, document number, name, nationality, date of birth, gender, and expiry date.


## Features
- MRZ detection in images
- Support for various MRZ formats (TD1, TD2, TD3)
- Text recognition and parsing

## Prerequisites
- A [trial license key](https://www.dynamsoft.com/customer/license/trialLicense/?product=dcv&package=cross-platform) for Dynamsoft Capture Vision
- C++ compiler 
- CMake

## Building the Application
1. Set the license key in `main.cpp`:

    ```cpp
    CLicenseManager::InitLicense("LICENSE-KEY", szErrorMsg, 256);
    ```

2. Create a build directory:

    ```bash
    mkdir build
    cd build
    ```

3. Configure with CMake and build:

    ```bash
    cmake ..
    cmake --build .
    ```