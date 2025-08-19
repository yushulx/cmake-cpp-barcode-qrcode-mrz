@echo off
setlocal enabledelayedexpansion
REM Windows batch script for building Qt Barcode Scanner
REM Usage: build.bat [Debug|Release]

set BUILD_TYPE=%1
if "%BUILD_TYPE%"=="" set BUILD_TYPE=Release

set PROJECT_DIR=%~dp0
set BUILD_DIR=%PROJECT_DIR%build

echo Building Qt Barcode Scanner in %BUILD_TYPE% mode...

REM Create build directory
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd /d "%BUILD_DIR%"

REM Check for Qt6_DIR and try to find Qt automatically
if "%Qt6_DIR%"=="" (
    echo Qt6_DIR environment variable not set. Searching for Qt installations...
    echo.
    
    set QT_FOUND=0
    
    REM Check for Qt in C:\Qt with MSVC 2019 (verified working)
    for /d %%i in (C:\Qt\6.*) do (
        if exist "%%i\msvc2019_64\lib\cmake\Qt6" (
            set "Qt6_DIR=%%i\msvc2019_64\lib\cmake\Qt6"
            set "CMAKE_PREFIX_PATH=%%i\msvc2019_64"
            set QT_FOUND=1
            echo Found Qt at: %%i\msvc2019_64\lib\cmake\Qt6
            goto build_start
        )
    )
    
    REM Check for Qt in C:\Qt with MSVC 2022
    for /d %%i in (C:\Qt\6.*) do (
        if exist "%%i\msvc2022_64\lib\cmake\Qt6" (
            set "Qt6_DIR=%%i\msvc2022_64\lib\cmake\Qt6"
            set "CMAKE_PREFIX_PATH=%%i\msvc2022_64"
            set QT_FOUND=1
            echo Found Qt at: %%i\msvc2022_64\lib\cmake\Qt6
            goto build_start
        )
    )
    
    REM Check for vcpkg Qt installation
    if defined VCPKG_ROOT (
        if exist "%VCPKG_ROOT%\installed\x64-windows\share\qt6" (
            set "Qt6_DIR=%VCPKG_ROOT%\installed\x64-windows\share\qt6"
            set "CMAKE_PREFIX_PATH=%VCPKG_ROOT%\installed\x64-windows"
            set QT_FOUND=1
            echo Found Qt via vcpkg at: %VCPKG_ROOT%\installed\x64-windows\share\qt6
            goto build_start
        )
    )
    
    if !QT_FOUND!==0 (
        echo Qt6 installation not found automatically.
        echo.
        echo Please install Qt6 or set Qt6_DIR environment variable manually:
        echo.
        echo Option 1: Install Qt using Qt Online Installer
        echo   Download from: https://www.qt.io/download-qt-installer
        echo   Install to C:\Qt\6.x.x\msvc2022_64\
        echo.
        echo Option 2: Install Qt using vcpkg
        echo   vcpkg install qt6-base qt6-multimedia qt6-multimediawidgets
        echo.
        echo Option 3: Set Qt6_DIR manually
        echo   set Qt6_DIR=C:\path\to\qt\lib\cmake\Qt6
        echo   Example: set Qt6_DIR=C:\Qt\6.5.0\msvc2022_64\lib\cmake\Qt6
        echo.
        pause
        exit /b 1
    )
)

:build_start
echo.
echo Using Qt6_DIR: %Qt6_DIR%
echo Using CMAKE_PREFIX_PATH: %CMAKE_PREFIX_PATH%
echo Configuring CMake...
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DCMAKE_PREFIX_PATH="%CMAKE_PREFIX_PATH%"
if errorlevel 1 (
    echo.
    echo CMake configuration failed!
    echo This might be due to:
    echo   1. Qt6 installation is incomplete or corrupt
    echo   2. Visual Studio 2022 is not installed
    echo   3. CMake is not in PATH
    echo.
    echo Troubleshooting:
    echo   - Verify Qt6 is properly installed
    echo   - Install Visual Studio 2022 with C++ development tools
    echo   - Install CMake and add to PATH
    echo.
    pause
    exit /b 1
)

echo.
echo Building project...
cmake --build . --config %BUILD_TYPE%
if errorlevel 1 (
    echo.
    echo Build failed!
    echo Check the error messages above for details.
    echo.
    pause
    exit /b 1
)

echo.
echo Build completed successfully!
echo Executable location: %BUILD_DIR%\bin\QtBarcodeScanner.exe
echo.
echo To run the application:
echo   cd %BUILD_DIR%\bin
echo   QtBarcodeScanner.exe
echo.
pause
