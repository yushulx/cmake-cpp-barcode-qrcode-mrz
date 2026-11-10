@echo off
echo Rust Barcode Reader UI (Slint)
echo =============================
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
echo Please add C:\Users\%USERNAME%\.cargo\bin to your PATH environment variable
pause
exit /b 1

:found_cargo

REM Run the built executable if present, otherwise build and run via cargo
if exist "target\release\barcode-reader-ui.exe" (
    echo Running built executable...
    echo.
    target\release\barcode-reader-ui.exe %*
) else (
    echo Built executable not found, building and running with cargo...
    echo.
    %CARGO_CMD% run --release --bin barcode-reader-ui -- %*
)

echo.
pause