@echo off
echo Rust Barcode Reader UI (Tauri)
echo ==============================
echo.

REM Check if Rust is installed in PATH first
where cargo >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    set CARGO_CMD=cargo
    goto :found_cargo
)

if exist "%USERPROFILE%\.cargo\bin\cargo.exe" (
    set CARGO_CMD="%USERPROFILE%\.cargo\bin\cargo.exe"
    echo Found Cargo at: %USERPROFILE%\.cargo\bin\cargo.exe
    goto :found_cargo
)

echo Error: Rust/Cargo not found in PATH or common locations
echo Please add %USERPROFILE%\.cargo\bin to your PATH environment variable
pause
exit /b 1

:found_cargo

REM The frontend is plain HTML/CSS/JS, so cargo alone can build and run the app.
REM Use "npm install && npm run dev" instead if you want the Tauri CLI.
cd /d "%~dp0src-tauri"
%CARGO_CMD% run --release

echo.
pause