@echo off
REM Arduino MCP Server Installation Script for Windows
REM Author: SURIOTA R&D Team

echo ================================
echo Arduino MCP Server Installation
echo ================================
echo.

REM Check Python installation
echo [1/5] Checking Python installation...
python --version >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] Python not found! Please install Python 3.10 or higher.
    echo Download from: https://www.python.org/downloads/
    pause
    exit /b 1
)
echo [OK] Python found
echo.

REM Check Arduino CLI installation
echo [2/5] Checking Arduino CLI installation...
arduino-cli version >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] Arduino CLI not found!
    echo.
    echo Please install Arduino CLI:
    echo   Option 1 - winget: winget install ArduinoSA.CLI
    echo   Option 2 - chocolatey: choco install arduino-cli
    echo   Option 3 - Manual: https://arduino.github.io/arduino-cli/latest/installation/
    pause
    exit /b 1
)
echo [OK] Arduino CLI found
arduino-cli version
echo.

REM Install Python dependencies
echo [3/5] Installing Python dependencies...
pip install -r requirements.txt
if %errorlevel% neq 0 (
    echo [ERROR] Failed to install dependencies
    pause
    exit /b 1
)
echo [OK] Dependencies installed
echo.

REM Check ESP32 board support
echo [4/5] Checking ESP32 board support...
arduino-cli core list | findstr "esp32:esp32" >nul 2>&1
if %errorlevel% neq 0 (
    echo [WARNING] ESP32 board support not installed
    echo Installing ESP32 board support...
    arduino-cli core update-index
    arduino-cli core install esp32:esp32
    if %errorlevel% neq 0 (
        echo [ERROR] Failed to install ESP32 board support
        pause
        exit /b 1
    )
)
echo [OK] ESP32 board support installed
echo.

REM Test MCP server
echo [5/5] Testing MCP server...
echo import sys > test_import.py
echo from tools.builder import ArduinoBuilder >> test_import.py
echo from tools.flasher import ArduinoFlasher >> test_import.py
echo from tools.monitor import SerialMonitor >> test_import.py
echo from tools.library_manager import LibraryManager >> test_import.py
echo print("All imports successful!") >> test_import.py

python test_import.py
if %errorlevel% neq 0 (
    echo [ERROR] MCP server imports failed
    del test_import.py
    pause
    exit /b 1
)
del test_import.py
echo [OK] MCP server ready
echo.

echo ================================
echo Installation Complete!
echo ================================
echo.
echo Next steps:
echo 1. Add this to your Claude Desktop config:
echo    %APPDATA%\Claude\claude_desktop_config.json
echo.
echo 2. Use the configuration from mcp_config_example.json
echo    (Update paths to match your installation)
echo.
echo 3. Restart Claude Desktop
echo.
echo 4. Test by asking Claude to "List connected Arduino boards"
echo.
pause
