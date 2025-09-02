@echo off
echo Rust Barcode Scanner
echo ===================
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

REM Check if built executable exists
if exist "target\release\dynamsoft-barcode-cli.exe" (
    echo Running built executable...
    echo.
    target\release\dynamsoft-barcode-cli.exe %*
) else (
    echo Built executable not found, running with cargo...
    echo.
    %CARGO_CMD% run -- %*
)

echo.
pause
