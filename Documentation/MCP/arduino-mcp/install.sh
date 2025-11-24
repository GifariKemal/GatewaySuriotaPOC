#!/bin/bash
# Arduino MCP Server Installation Script for Linux/macOS
# Author: SURIOTA R&D Team

set -e

echo "================================"
echo "Arduino MCP Server Installation"
echo "================================"
echo ""

# Check Python installation
echo "[1/5] Checking Python installation..."
if ! command -v python3 &> /dev/null; then
    echo "[ERROR] Python 3 not found! Please install Python 3.10 or higher."
    echo "Download from: https://www.python.org/downloads/"
    exit 1
fi
PYTHON_VERSION=$(python3 --version)
echo "[OK] $PYTHON_VERSION found"
echo ""

# Check Arduino CLI installation
echo "[2/5] Checking Arduino CLI installation..."
if ! command -v arduino-cli &> /dev/null; then
    echo "[ERROR] Arduino CLI not found!"
    echo ""
    echo "Please install Arduino CLI:"
    echo "  curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh"
    echo ""
    echo "Then add to PATH or move to /usr/local/bin"
    exit 1
fi
echo "[OK] Arduino CLI found"
arduino-cli version
echo ""

# Install Python dependencies
echo "[3/5] Installing Python dependencies..."
pip3 install -r requirements.txt
echo "[OK] Dependencies installed"
echo ""

# Check ESP32 board support
echo "[4/5] Checking ESP32 board support..."
if ! arduino-cli core list | grep -q "esp32:esp32"; then
    echo "[WARNING] ESP32 board support not installed"
    echo "Installing ESP32 board support..."
    arduino-cli core update-index
    arduino-cli core install esp32:esp32
fi
echo "[OK] ESP32 board support installed"
echo ""

# Test MCP server
echo "[5/5] Testing MCP server..."
cat > test_import.py << EOF
import sys
from tools.builder import ArduinoBuilder
from tools.flasher import ArduinoFlasher
from tools.monitor import SerialMonitor
from tools.library_manager import LibraryManager
print("All imports successful!")
EOF

python3 test_import.py
rm test_import.py
echo "[OK] MCP server ready"
echo ""

echo "================================"
echo "Installation Complete!"
echo "================================"
echo ""
echo "Next steps:"
echo "1. Add this to your Claude Desktop config:"
echo "   macOS: ~/Library/Application Support/Claude/claude_desktop_config.json"
echo "   Linux: ~/.config/Claude/claude_desktop_config.json"
echo ""
echo "2. Use the configuration from mcp_config_example.json"
echo "   (Update paths to match your installation)"
echo ""
echo "3. Restart Claude Desktop"
echo ""
echo "4. Test by asking Claude to 'List connected Arduino boards'"
echo ""

# Make this script executable
chmod +x "$0"
