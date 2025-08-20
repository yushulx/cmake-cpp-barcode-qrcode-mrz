# Qt Barcode Scanner

A complete cross-platform GUI barcode scanner application built with Qt 6 and Dynamsoft Capture Vision SDK.

## Prerequisites

### Required Dependencies

1. **Qt 6.2 or later**
   - Qt Core
   - Qt Widgets  
   - Qt Multimedia
   - Qt MultimediaWidgets

2. **OpenCV 4.5 or later** (for camera streaming and image loading)

3. **Dynamsoft Capture Vision SDK** (included in project)
   - DynamsoftCaptureVisionRouter
   - DynamsoftBarcodeReader
   - DynamsoftCore
   - DynamsoftUtility
   - DynamsoftLicense

4. **CMake 3.16 or later**

5. **C++17 compatible compiler**
   - Windows: MSVC 2019/2022
   - Linux: GCC 8+ 
   - macOS: Clang 10+

### Platform-Specific Setup

#### Windows (MSVC)

1. **Install Qt 6.7.2 or later**
   - Download from https://www.qt.io/download-qt-installer
   - Install with MSVC 2019/2022 64-bit component
   - Ensure Qt Multimedia is included

2. **Install OpenCV**
   - Download OpenCV 4.8.0 from https://opencv.org/releases/
   - Extract to `C:\opencv` or set `OpenCV_DIR` environment variable

3. **Set Environment Variables**:
   ```cmd
   set Qt6_DIR=C:\Qt\6.7.2\msvc2022_64\lib\cmake\Qt6
   set OpenCV_DIR=C:\opencv\build
   ```

#### Linux (GCC)
```bash
# Ubuntu/Debian
sudo apt-get install qt6-base-dev qt6-multimedia-dev libopencv-dev

# Fedora/CentOS
sudo dnf install qt6-qtbase-devel qt6-qtmultimedia-devel opencv-devel
```

#### macOS (Clang)
```bash
# Using Homebrew
brew install qt@6 opencv
export Qt6_DIR=/opt/homebrew/lib/cmake/Qt6
export OpenCV_DIR=/opt/homebrew/lib/cmake/opencv4
```

## Building

### Using CMake

```bash
# Create build directory
mkdir build && cd build

# Configure
cmake .. -DQt6_DIR=/path/to/qt6/lib/cmake/Qt6 -DOpenCV_DIR=/path/to/opencv

# Build
cmake --build . --config Release
```

### Platform-Specific Build Commands

#### Windows
```cmd
mkdir build
cd build
set Qt6_DIR=C:\Qt\6.7.2\msvc2022_64\lib\cmake\Qt6
set OpenCV_DIR=C:\opencv\build
cmake .. -DQt6_DIR="%Qt6_DIR%" -DOpenCV_DIR="%OpenCV_DIR%" -G "Visual Studio 17 2022"
cmake --build . --config Release
```

#### Linux
```bash
mkdir build && cd build
cmake .. -DQt6_DIR=/usr/lib/x86_64-linux-gnu/cmake/Qt6
make -j$(nproc)
```

#### macOS
```bash
mkdir build && cd build
cmake .. -DQt6_DIR=/opt/homebrew/lib/cmake/Qt6 -DOpenCV_DIR=/opt/homebrew/lib/cmake/opencv4
make -j$(sysctl -n hw.ncpu)
```

## Usage

### Running the Application

After building, the executable will be in the build directory:

```bash
# Windows
./QtBarcodeScanner.exe

# Linux/macOS  
./QtBarcodeScanner
```

![Qt barcode scanner](https://www.dynamsoft.com/codepool/img/2025/08/qt-barcode-scanner-windows-gui-app.png)

## Configuration

### License Key Setup

1. Go to **Settings > Set License Key** 
2. Enter your Dynamsoft Capture Vision license key
3. The default key "LICENSE-KEY" is for evaluation purposes only
4. License status is displayed in the application status bar

### File Paths

The application requires the following files in the executable directory:
- `DynamsoftCore.dll`, `DynamsoftBarcodeReader.dll`, etc. (Windows)
- Qt platform plugins in `platforms/` subdirectory
- Qt image format plugins in `imageformats/` subdirectory
- Dynamsoft resource files in `Templates/` and `Models/` subdirectories

*Note: CMake automatically copies all required files during build.*

