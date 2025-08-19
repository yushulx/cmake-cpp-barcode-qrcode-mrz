#!/bin/bash

# Cross-platform build script for Qt Barcode Scanner
# Usage: ./build.sh [Debug|Release]

BUILD_TYPE=${1:-Release}
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$PROJECT_DIR/build"

echo "Building Qt Barcode Scanner in $BUILD_TYPE mode..."

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Platform-specific Qt path detection
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "win32" ]]; then
    # Windows
    echo "Detected Windows platform"
    if [ -z "$Qt6_DIR" ]; then
        # Try to find Qt in common Windows locations
        for qt_dir in /c/Qt/6.*/msvc2019_64 /c/Qt/6.*/msvc2022_64; do
            if [ -d "$qt_dir/lib/cmake/Qt6" ]; then
                export Qt6_DIR="$qt_dir/lib/cmake/Qt6"
                export CMAKE_PREFIX_PATH="$qt_dir"
                echo "Found Qt at: $qt_dir"
                break
            fi
        done
        
        if [ -z "$Qt6_DIR" ]; then
            echo "Please set Qt6_DIR environment variable to your Qt installation"
            echo "Example: export Qt6_DIR='C:/Qt/6.7.2/msvc2019_64/lib/cmake/Qt6'"
            exit 1
        fi
    fi
    cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DCMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH"
    cmake --build . --config $BUILD_TYPE
elif [[ "$OSTYPE" == "darwin"* ]]; then
    # macOS
    echo "Detected macOS platform" 
    if [ -z "$Qt6_DIR" ]; then
        # Try to find Qt in common locations
        if [ -d "/opt/homebrew/lib/cmake/Qt6" ]; then
            export Qt6_DIR="/opt/homebrew/lib/cmake/Qt6"
        elif [ -d "/usr/local/lib/cmake/Qt6" ]; then
            export Qt6_DIR="/usr/local/lib/cmake/Qt6"
        else
            echo "Qt6 not found. Please install Qt6 or set Qt6_DIR"
            echo "Install with: brew install qt@6"
            exit 1
        fi
    fi
    cmake .. -DQt6_DIR="$Qt6_DIR"
    make -j$(sysctl -n hw.ncpu)
else
    # Linux
    echo "Detected Linux platform"
    if [ -z "$Qt6_DIR" ]; then
        # Try to find Qt in common locations
        if [ -d "/usr/lib/x86_64-linux-gnu/cmake/Qt6" ]; then
            export Qt6_DIR="/usr/lib/x86_64-linux-gnu/cmake/Qt6"
        elif [ -d "/usr/lib64/cmake/Qt6" ]; then
            export Qt6_DIR="/usr/lib64/cmake/Qt6"
        else
            echo "Qt6 not found. Please install Qt6 development packages"
            echo "Ubuntu/Debian: sudo apt-get install qt6-base-dev qt6-multimedia-dev"
            echo "Fedora: sudo dnf install qt6-qtbase-devel qt6-qtmultimedia-devel"
            exit 1
        fi
    fi
    cmake .. -DQt6_DIR="$Qt6_DIR" -DCMAKE_BUILD_TYPE=$BUILD_TYPE
    make -j$(nproc)
fi

if [ $? -eq 0 ]; then
    echo "Build completed successfully!"
    
    if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "win32" ]]; then
        echo "Executable location: $BUILD_DIR/bin/QtBarcodeScanner.exe"
        echo "Run: cd $BUILD_DIR/bin && ./QtBarcodeScanner.exe"
    else
        echo "Executable location: $BUILD_DIR/QtBarcodeScanner"
        echo "Run: ./QtBarcodeScanner"
    fi
else
    echo "Build failed!"
    exit 1
fi
