# Quick Start Guide - Arduino MCP Server

**Get started in 5 minutes!**

---

## ⚡ Quick Installation (Windows)

### 1. Install Arduino CLI

Choose one method:

**Option A - Using winget (recommended):**

```powershell
winget install ArduinoSA.CLI
```

**Option B - Using Chocolatey:**

```powershell
choco install arduino-cli
```

**Option C - Manual download:** Download from:
https://arduino.github.io/arduino-cli/latest/installation/

### 2. Run Installation Script

```bash
cd C:\Users\Administrator\Music\GatewaySuriotaPOC\arduino-mcp
install.bat
```

This will:

- ✅ Check Python installation
- ✅ Verify Arduino CLI
- ✅ Install Python dependencies
- ✅ Install ESP32 board support
- ✅ Test the MCP server

### 3. Configure Claude Desktop

**Edit:** `%APPDATA%\Claude\claude_desktop_config.json`

**Add:**

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

### 4. Restart Claude Desktop

Close and reopen Claude Desktop to load the MCP server.

### 5. Test It!

Ask Claude:

```
List all connected Arduino boards
```

---

## 🎯 Common Tasks

### Build the Gateway Project

```
Build the Main project at C:/Users/Administrator/Music/GatewaySuriotaPOC/Main
```

### Upload Firmware

```
Upload the firmware to my ESP32-S3 board
```

### Monitor Serial Output

```
Monitor the serial output at 115200 baud for 30 seconds
```

### Install Required Libraries

```
Install ArduinoJson library version 7.4.2
```

---

## 🐛 Troubleshooting

### "arduino-cli not found"

**Solution:** Install Arduino CLI and add to PATH:

```powershell
# Check installation
arduino-cli version

# If not found, install with winget
winget install ArduinoSA.CLI

# Restart terminal
```

### "No boards detected"

**Solution:**

1. Connect ESP32-S3 via USB
2. Install USB drivers (CP210x or CH340)
3. Check Device Manager for COM port
4. Try: `arduino-cli board list`

### "MCP server not responding"

**Solution:**

1. Check config file paths are absolute paths
2. Restart Claude Desktop
3. Check Python is installed: `python --version`
4. Verify dependencies: `pip list | findstr mcp`

---

## 📚 Learn More

- **Full Documentation:** `README.md`
- **Installation Scripts:** `install.bat` (Windows) or `install.sh`
  (Linux/macOS)
- **Configuration Example:** `mcp_config_example.json`
- **Gateway Project Docs:** `../Documentation/`

---

## 🆘 Need Help?

Check the full `README.md` for:

- Complete tool reference
- Advanced usage examples
- Detailed troubleshooting
- Development guide

---

**Happy coding with Arduino MCP! 🚀**
