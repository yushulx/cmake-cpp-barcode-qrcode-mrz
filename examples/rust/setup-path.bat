@echo off
echo Adding Rust/Cargo to PATH...
echo.

REM Check if Cargo exists in the expected location
if not exist "C:\Users\%USERNAME%\.cargo\bin\cargo.exe" (
    echo Error: Cargo not found at C:\Users\%USERNAME%\.cargo\bin\cargo.exe
    echo Please make sure Rust is properly installed
    pause
    exit /b 1
)

echo Found Cargo at: C:\Users\%USERNAME%\.cargo\bin\cargo.exe
echo.

REM Add to PATH for current session
set PATH=%PATH%;C:\Users\%USERNAME%\.cargo\bin
echo Added to PATH for current session.

REM Add to PATH permanently (requires admin rights)
echo.
echo To add Cargo to PATH permanently, you have two options:
echo.
echo Option 1 (Manual - Recommended):
echo   1. Press Win + R, type 'sysdm.cpl' and press Enter
echo   2. Click 'Environment Variables...'
echo   3. Under 'User variables', select 'Path' and click 'Edit...'
echo   4. Click 'New' and add: C:\Users\%USERNAME%\.cargo\bin
echo   5. Click OK on all dialogs
echo   6. Restart your terminal/command prompt
echo.
echo Option 2 (Automatic - requires Administrator privileges):
echo   Run this script as Administrator to automatically add to PATH
echo.

REM Check if running as admin
net session >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo Running as Administrator - adding to PATH permanently...
    setx PATH "%PATH%;C:\Users\%USERNAME%\.cargo\bin" /M
    if %ERRORLEVEL% EQU 0 (
        echo Successfully added to system PATH!
        echo Please restart your terminal for changes to take effect.
    ) else (
        echo Failed to add to system PATH. Please use manual method above.
    )
) else (
    echo Not running as Administrator - PATH added for current session only.
    echo Use manual method above for permanent addition.
)

echo.
echo Testing Cargo...
cargo --version
if %ERRORLEVEL% EQU 0 (
    echo Cargo is now accessible!
) else (
    echo Cargo still not accessible. Please restart your terminal.
)

echo.
pause
