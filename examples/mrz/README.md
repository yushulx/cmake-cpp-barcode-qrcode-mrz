# Desktop Passport MRZ Scanner for Windows and Linux
This C/C++ sample demonstrates how to implement **MRZ (Machine Readable Zone)** recognition using the Dynamsoft Capture Vision SDK.

https://github.com/user-attachments/assets/71bbe660-eabc-4d27-965f-1e03871df974

## Prerequisites
- [CMake](https://cmake.org/download/)
- [OpenCV 4.5.0](https://opencv.org/releases/)
- [Dynamsoft Capture Vision Trial License](https://www.dynamsoft.com/customer/license/trialLicense/?product=dcv&package=cross-platform)


## Getting Started
1. Set the license key in `main.cxx`:

    ```cpp
    iRet = CLicenseManager::InitLicense("LICENSE-KEY", szErrorMsg, 256);
    ```

2. Build and run the application:

    ```
    mkdir build
    cd build
    cmake ..
    # For OpenCV
    # cmake -DCMAKE_GENERATOR_PLATFORM=x64 -DENABLE_OPENCV=TRUE ..
    cmake --build .
    ```

    ![desktop cpp MRZ scanner](https://www.dynamsoft.com/codepool/img/2024/09/desktop-cpp-mrz-scanner.png)
    
    
 ## Blog
 [Passport MRZ Recognition with Dynamsoft C++ OCR SDK](https://www.dynamsoft.com/codepool/passport-mrz-recognition-cpp-windows-linux.html)
