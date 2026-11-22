@echo off
echo Building Rust Barcode Scanner...
echo.

REM Check if Rust is installed in PATH first
where cargo >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    set CARGO_CMD=cargo
    goto :found_cargo
)

REM Check common Rust installation locations
if exist "C:\Users\%USERNAME%\.cargo\bin\cargo.exe" (
    set CARGO_CMD="C:\Users\%USERNAME%\.cargo\bin\cargo.exe"
    echo Found Cargo at: C:\Users\%USERNAME%\.cargo\bin\cargo.exe
    goto :found_cargo
)

if exist "%USERPROFILE%\.cargo\bin\cargo.exe" (
    set CARGO_CMD="%USERPROFILE%\.cargo\bin\cargo.exe"
    echo Found Cargo at: %USERPROFILE%\.cargo\bin\cargo.exe
    goto :found_cargo
)

echo Error: Rust/Cargo not found in PATH or common locations
echo Please either:
echo   1. Add C:\Users\%USERNAME%\.cargo\bin to your PATH environment variable, or
echo   2. Install Rust from https://rustup.rs/
echo   3. Restart your terminal after installation
pause
exit /b 1

:found_cargo

echo Rust found, proceeding with build...
echo.

REM Clean previous builds
echo Cleaning previous builds...
%CARGO_CMD% clean

REM Build the project
echo Building in release mode...
%CARGO_CMD% build --release

if %ERRORLEVEL% EQU 0 (
    echo.
    echo Build successful!
    echo.
    echo You can now run the program with:
    echo   %CARGO_CMD% run --bin barcode-scanner                       (CLI interactive mode)
    echo   %CARGO_CMD% run --bin barcode-scanner -- "path/to/image.jpg"   (CLI single file)
    echo   %CARGO_CMD% run --release --bin barcode-reader-ui           (Slint desktop UI)
    echo.
    echo Or use the built executables:
    echo   target\release\barcode-scanner.exe
    echo   target\release\barcode-reader-ui.exe
    echo.
) else (
    echo.
    echo Build failed! Please check the error messages above.
    echo.
)

pause
