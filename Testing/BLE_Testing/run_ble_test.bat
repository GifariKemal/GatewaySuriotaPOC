@echo off
REM ============================================================================
REM BLE Testing Tool Launcher
REM ============================================================================

echo.
echo ========================================================================
echo BLE Testing Tool for SRT-MGATE-1210
echo ========================================================================
echo.

REM Check if Python is installed
python --version >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Python is not installed or not in PATH
    echo Please install Python 3.7+ from https://www.python.org/
    echo.
    pause
    exit /b 1
)

echo [INFO] Python detected
python --version

REM Check if bleak is installed
python -c "import bleak" >nul 2>&1
if errorlevel 1 (
    echo.
    echo [WARNING] 'bleak' library not found
    echo [INFO] Installing dependencies...
    echo.
    pip install -r requirements.txt
    if errorlevel 1 (
        echo.
        echo [ERROR] Failed to install dependencies
        pause
        exit /b 1
    )
)

echo.
echo [INFO] All dependencies installed
echo [INFO] Starting BLE Testing Tool...
echo.
echo ========================================================================
echo.

REM Run the BLE test tool
python ble_test.py

echo.
echo ========================================================================
echo BLE Testing Tool Exited
echo ========================================================================
pause
