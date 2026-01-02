# Arduino MCP Server

**Model Context Protocol (MCP) server for Arduino CLI operations**

Build, upload, and monitor Arduino/ESP32 projects through LLM communication
using the Model Context Protocol.

---

## 🎯 Features

- ✅ **Build Projects**: Compile Arduino sketches with detailed error reporting
- ✅ **Upload Firmware**: Flash compiled binaries to connected boards
- ✅ **Serial Monitor**: Capture serial output from boards
- ✅ **Library Management**: Install, search, and manage Arduino libraries
- ✅ **Board Detection**: Auto-detect connected Arduino/ESP32 boards
- ✅ **Auto-Fix Suggestions**: Get intelligent suggestions for common
  compilation errors
- ✅ **ESP32-S3 Support**: Optimized for ESP32-S3 boards (configurable for other
  boards)

---

## 📋 Prerequisites

### 1. Python 3.10 or higher

Check your Python version:

```bash
python --version
```

If not installed, download from [python.org](https://www.python.org/downloads/)

### 2. Arduino CLI

Install Arduino CLI from
[arduino.github.io/arduino-cli](https://arduino.github.io/arduino-cli/latest/installation/)

**Windows:**

```powershell
# Using winget
winget install ArduinoSA.CLI

# Or using chocolatey
choco install arduino-cli
```

**Linux/macOS:**

```bash
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
```

Verify installation:

```bash
arduino-cli version
```

### 3. ESP32 Board Support (for ESP32-S3 projects)

Install ESP32 board support:

```bash
arduino-cli core update-index
arduino-cli core install esp32:esp32
```

---

## 🚀 Installation

### 1. Navigate to the MCP server directory

```bash
cd C:\Users\Administrator\Music\GatewaySuriotaPOC\arduino-mcp
```

### 2. Install Python dependencies

```bash
pip install -r requirements.txt
```

Or with virtual environment (recommended):

```bash
python -m venv venv
venv\Scripts\activate  # Windows
# source venv/bin/activate  # Linux/macOS

pip install -r requirements.txt
```

### 3. Verify arduino-cli is accessible

```bash
arduino-cli version
```

---

## ⚙️ Configuration

### For Claude Desktop

Add this configuration to your Claude Desktop config file:

**Windows:** `%APPDATA%\Claude\claude_desktop_config.json` **macOS:**
`~/Library/Application Support/Claude/claude_desktop_config.json` **Linux:**
`~/.config/Claude/claude_desktop_config.json`

```json
{
  "mcpServers": {
    "arduino-mcp": {
      "command": "python",
      "args": [
        "C:/Users/Administrator/Music/GatewaySuriotaPOC/arduino-mcp/main.py"
      ],
      "env": {
        "PROJECT_PATH": "C:/Users/Administrator/Music/GatewaySuriotaPOC/Main"
      }
    }
  }
}
```

**Important:** Update the paths to match your actual installation directory.

### For other MCP clients

Use the `mcp_config_example.json` as a template and adjust paths accordingly.

---

## 🔧 Available Tools

### 1. `arduino_build`

Compile an Arduino project.

**Parameters:**

- `project_path` (required): Path to project directory containing .ino file
- `fqbn` (optional): Fully Qualified Board Name (default: `esp32:esp32:esp32s3`)
- `verbose` (optional): Enable verbose build output (default: `false`)

**Example:**

```
Build the project at C:/Users/Administrator/Music/GatewaySuriotaPOC/Main
```

### 2. `arduino_upload`

Upload compiled firmware to connected board.

**Parameters:**

- `project_path` (required): Path to project directory
- `port` (optional): Serial port (auto-detect if not specified)
- `fqbn` (optional): Fully Qualified Board Name (default: `esp32:esp32:esp32s3`)
- `verify` (optional): Verify upload after writing (default: `true`)

**Example:**

```
Upload the firmware from Main directory to the connected ESP32-S3 board
```

### 3. `arduino_monitor`

Open serial monitor to view board output.

**Parameters:**

- `port` (required): Serial port to monitor
- `baudrate` (optional): Baud rate (default: `115200`)
- `duration` (optional): Duration in seconds (default: `10`, `0` = instructions
  only)

**Example:**

```
Monitor serial output from COM3 at 115200 baud for 20 seconds
```

### 4. `arduino_board_list`

List all connected Arduino boards and their ports.

**Example:**

```
List all connected Arduino boards
```

### 5. `arduino_lib_install`

Install Arduino library.

**Parameters:**

- `library_name` (required): Name of library to install
- `version` (optional): Specific version (default: latest)

**Example:**

```
Install ArduinoJson library version 7.4.2
```

### 6. `arduino_lib_search`

Search for Arduino libraries.

**Parameters:**

- `query` (required): Search query

**Example:**

```
Search for MQTT libraries
```

### 7. `arduino_clean`

Clean build artifacts from project.

**Parameters:**

- `project_path` (required): Path to project directory

**Example:**

```
Clean the build directory for the Main project
```

### 8. `arduino_compile_fix`

Attempt to automatically fix compilation errors.

**Parameters:**

- `project_path` (required): Path to project directory
- `error_log` (required): Compilation error log from previous build

**Example:**

```
Analyze and suggest fixes for the compilation errors in the build log
```

---

## 📝 Usage Examples

### Example 1: Build and Upload

**You:** _"Build the SRT-MGATE-1210 project and upload it to the connected
ESP32-S3"_

**LLM will:**

1. Use `arduino_build` to compile the project
2. Check for compilation errors
3. If successful, use `arduino_upload` to flash firmware
4. Report upload status

### Example 2: Debug Serial Output

**You:** _"Monitor the serial output from my ESP32 board on COM3"_

**LLM will:**

1. Use `arduino_board_list` to verify port
2. Use `arduino_monitor` to capture serial output
3. Display the captured logs

### Example 3: Install Missing Library

**You:** _"The build failed because ArduinoJson is missing. Install it."_

**LLM will:**

1. Use `arduino_lib_install` to install ArduinoJson
2. Verify installation
3. Suggest rebuilding the project

### Example 4: Auto-Fix Compilation Errors

**You:** _"The build failed with errors. Can you help fix it?"_

**LLM will:**

1. Analyze the error log
2. Use `arduino_compile_fix` to suggest fixes
3. Implement fixes if possible
4. Rebuild the project

---

## 🔍 Troubleshooting

### Issue: "arduino-cli not found"

**Solution:** Ensure Arduino CLI is installed and in your PATH.

**Windows:**

```powershell
# Add to PATH or use full path
where arduino-cli
```

**Linux/macOS:**

```bash
which arduino-cli
```

### Issue: "No boards detected"

**Solution:**

1. Connect your ESP32-S3 board via USB
2. Install USB-to-Serial drivers (CP210x or CH340)
3. Check Device Manager (Windows) or `ls /dev/tty*` (Linux/macOS)
4. Verify port access permissions

### Issue: "Permission denied on port"

**Solution:**

**Windows:** Run as Administrator or close other programs using the port

**Linux:** Add user to dialout group:

```bash
sudo usermod -a -G dialout $USER
# Logout and login again
```

**macOS:** Grant terminal permissions in System Preferences → Security & Privacy

### Issue: "Compilation failed with unknown errors"

**Solution:**

1. Use `arduino_build` with `verbose: true` for detailed errors
2. Check that all required libraries are installed
3. Verify ESP32 board support is installed: `arduino-cli core list`
4. Use `arduino_compile_fix` for automatic suggestions

### Issue: "MCP server not responding in Claude Desktop"

**Solution:**

1. Check that paths in `claude_desktop_config.json` are absolute paths
2. Restart Claude Desktop after configuration changes
3. Check Claude Desktop logs for errors
4. Verify Python and dependencies are installed: `pip list | grep mcp`

---

## 🏗️ Project Structure

```
arduino-mcp/
├── main.py                    # MCP server entry point
├── requirements.txt           # Python dependencies
├── mcp_config_example.json    # Example MCP configuration
├── README.md                  # This file
└── tools/
    ├── __init__.py            # Tools package init
    ├── builder.py             # Arduino build/compile tool
    ├── flasher.py             # Firmware upload tool
    ├── monitor.py             # Serial monitor tool
    └── library_manager.py     # Library management tool
```

---

## 🔗 Integration with SRT-MGATE-1210 Gateway

This MCP server is specifically designed for the **SRT-MGATE-1210 Industrial IoT
Gateway** project.

### Quick Start for Gateway Project

1. **Build the firmware:**

   ```
   Build the Main project at C:/Users/Administrator/Music/GatewaySuriotaPOC/Main
   ```

2. **Upload to ESP32-S3:**

   ```
   Upload the firmware to the connected ESP32-S3 board
   ```

3. **Monitor serial output:**
   ```
   Monitor COM3 at 115200 baud for 30 seconds
   ```

### Required Libraries for Gateway

The following libraries are required for the SRT-MGATE-1210 project:

```
ArduinoJson @ 7.4.2
RTClib @ 2.1.4
NTPClient @ 3.2.1
Ethernet @ 2.0.2
TBPubSubClient @ 2.12.1
ModbusMaster @ 2.0.1
OneButton @ 2.0+
ArduinoHttpClient @ 0.6.1
```

**Install all at once:**

```
Install these libraries: ArduinoJson, RTClib, NTPClient, Ethernet, TBPubSubClient, ModbusMaster, OneButton, ArduinoHttpClient
```

---

## 🛠️ Development

### Running in Development Mode

For development and debugging, run the MCP server directly:

```bash
python main.py
```

The server will start in stdio mode and wait for MCP protocol messages.

### Testing Tools Independently

You can test individual tools by importing them:

```python
from tools.builder import ArduinoBuilder
import asyncio

async def test():
    builder = ArduinoBuilder()
    result = await builder.build(
        project_path="C:/path/to/project/Main",
        fqbn="esp32:esp32:esp32s3",
        verbose=True
    )
    print(result)

asyncio.run(test())
```

---

## 📚 Additional Resources

- **Arduino CLI Documentation:**
  [arduino.github.io/arduino-cli](https://arduino.github.io/arduino-cli/)
- **Model Context Protocol:**
  [modelcontextprotocol.io](https://modelcontextprotocol.io/)
- **ESP32-S3 Documentation:**
  [docs.espressif.com](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/)
- **Gateway Project Docs:** `../Documentation/`

---

## 🤝 Contributing

This MCP server is part of the SRT-MGATE-1210 Gateway project. For issues or
improvements:

1. Check existing documentation in `../Documentation/`
2. Follow the coding conventions in `../CLAUDE.md`
3. Test with the actual Gateway hardware
4. Update this README if adding new tools

---

## 📄 License

This MCP server follows the same license as the SRT-MGATE-1210 Gateway project
(MIT License).

---

## 👨‍💻 Author

**SURIOTA R&D Team**

- **Developer:** Kemal
- **Project:** SRT-MGATE-1210 Industrial IoT Gateway
- **Version:** 1.0.0
- **Last Updated:** November 24, 2025

---

**Made with ❤️ for Arduino automation and ESP32 development**
