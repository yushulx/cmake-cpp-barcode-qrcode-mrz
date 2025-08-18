# Qt Desktop Passport Scanner
A desktop passport scanner app built with [Dynamsoft OCR SDK](https://www.dynamsoft.com/label-recognition/overview/), [Qt](https://www.qt.io/) and a USB web camera.

## Trial License
Get a [trial license](https://www.dynamsoft.com/customer/license/trialLicense/?product=dcv&package=cross-platform) to activate passport scanning functions.

![sdk license key](https://www.dynamsoft.com/blog/wp-content/uploads/2021/09/passport-scanner-license-key.png)
 
## Download
- Qt
  - [Windows](https://www.qt.io/download)
  - Linux
    
    ```bash
    sudo apt-get install qt5-default qtmultimedia5-dev
    ```
    
- [Dynamsoft Label Recognizer 2.0 for Windows and Linux](https://www.dynamsoft.com/barcode-reader/downloads/)

## Build and Run

![Qt desktop passport scanner](https://www.dynamsoft.com/blog/wp-content/uploads/2021/09/passport-scanner-qt-mrz.png)

**Windows**

### Option 1: MSVC (Recommended for Windows)

#### Prerequisites
- Visual Studio 2019 or 2022 with C++ support
- Qt 5.15+ with MSVC build variant (e.g., MSVC2019_64)
- CMake 3.5+

#### Build Steps
```bash
mkdir build
cd build

# Configure with Qt5_DIR pointing to your MSVC Qt installation
cmake -G "Visual Studio 17 2022" -A x64 -DQt5_DIR="C:\Qt\5.15.2\msvc2019_64\lib\cmake\Qt5" ..

# Build the project
cmake --build . --config Debug
```

#### Runtime Setup
The CMakeLists.txt automatically copies required Qt dependencies during build for MSVC:

- **Qt DLLs**: Automatically copied using windeployqt or manual copying
- **Qt Plugins**: Platform and multimedia plugins copied to appropriate subdirectories
- **Dynamsoft DLLs**: Copied from platform/windows/bin/

#### Running the Application
```batch
# Navigate to build output directory
cd Debug  # or Release

# Run directly - Qt dependencies are already in place
MRZRecognizer.exe
```

**Note**: If you encounter plugin loading issues, you can set the plugin path explicitly:
```batch
set QT_PLUGIN_PATH=%CD%
MRZRecognizer.exe
```

### Option 2: MinGW

Add `Qt/5.12.11/mingw73_64/bin` and `Qt/Tools/mingw73_64/bin` to system path.

```bash
mkdir build
cd build
cmake -G "MinGW Makefiles" ..
cmake --build .
MRZRecognizer.exe
```

**Linux**

```bash
mkdir build
cd build
cmake ..
cmake --build .
./MRZRecognizer
```
 
## Blog
[Building Desktop Passport Scanner with Qt and USB Camera](https://www.dynamsoft.com/codepool/passport-scanner-qt-desktop-camera.html)
