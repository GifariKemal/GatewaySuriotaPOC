@echo off
echo ============================================================
echo SRT-MGATE-1210 RTU Testing - Create Device + 5 Registers
echo ============================================================
echo.

REM Check if Python is installed
python --version >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] Python not found! Please install Python 3.8+
    pause
    exit /b 1
)

REM Check if bleak is installed
python -c "import bleak" >nul 2>&1
if %errorlevel% neq 0 (
    echo [INFO] Installing required dependencies...
    pip install -r requirements.txt
    if %errorlevel% neq 0 (
        echo [ERROR] Failed to install dependencies
        pause
        exit /b 1
    )
)

echo [INFO] Running RTU device creation script...
echo.
python create_device_5_registers.py

echo.
echo ============================================================
pause
