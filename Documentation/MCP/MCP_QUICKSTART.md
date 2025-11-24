# MCP Quick Start Guide

**Get Started with MCP Servers in 10 Minutes**

**Project:** SRT-MGATE-1210 Gateway | **Version:** 1.0.0 | **Date:** Nov 24, 2025

---

## ⚡ Quick Install (5 Minutes)

### Prerequisites Check

```bash
# Check Node.js (required for Context7 & Playwright)
node --version   # Should be v18+

# Check Arduino CLI (required for Arduino MCP)
arduino-cli version

# Check Python (required for Arduino MCP)
python --version  # Should be 3.10+
```

### Install All MCP Servers

```bash
# 1. Install Arduino MCP
cd C:\Users\Administrator\Music\GatewaySuriotaPOC\arduino-mcp
install.bat

# 2. Install Context7
claude mcp add context7 -- npx -y @upstash/context7-mcp

# 3. Install Playwright
claude mcp add playwright -- npx @playwright/mcp@latest

# 4. Verify all installed
claude mcp list
```

**Expected Output:**
```
context7: npx -y @upstash/context7-mcp - ✓ Connected
playwright: npx @playwright/mcp@latest - ✓ Connected
```

---

## 🚀 Quick Usage Examples

### Example 1: Build & Upload Firmware (Arduino MCP)

```
Build the Main project and upload to ESP32-S3
```

**What happens:**
1. Arduino MCP compiles firmware
2. Auto-detects COM port
3. Uploads to board
4. Reports success

⏱️ **Time:** ~2 minutes

### Example 2: Get Latest Documentation (Context7)

```
Show me the latest ArduinoJson 7.x API for handling large JSON files with PSRAM.
use context7
```

**What happens:**
1. Context7 fetches ArduinoJson 7.x docs
2. Returns current API + examples
3. Includes PSRAM optimization tips

⏱️ **Time:** ~30 seconds

### Example 3: Test Web Interface (Playwright)

```
Open http://localhost:8080 and verify the Modbus simulator is running
```

**What happens:**
1. Playwright launches browser
2. Navigates to URL
3. Checks page loaded
4. Returns status

⏱️ **Time:** ~15 seconds

---

## 🎯 Common Workflows

### Workflow 1: Add New Feature

```
Step 1: Research
"Show me ESP32 BLE GATT server examples. use context7"

Step 2: Implement
"Add BLE notification support to BLEManager.cpp"

Step 3: Build & Test
"Build and upload firmware, then monitor serial for 30 seconds"

Step 4: Document
"Update Documentation/API_Reference/API.md with new BLE notification API"
```

⏱️ **Total Time:** ~30 minutes (vs 2-3 hours manually)

### Workflow 2: Fix Bug

```
Step 1: Get logs
"Build debug firmware and monitor serial output"

Step 2: Research
"What does PubSubClient error code -2 mean? use context7"

Step 3: Fix
"Implement MQTT reconnection logic with exponential backoff"

Step 4: Test
"Build, upload, and verify MQTT reconnects successfully"
```

⏱️ **Total Time:** ~15 minutes (vs 1-2 hours manually)

### Workflow 3: Test Web Dashboard

```
Step 1: Test UI
"Open http://192.168.1.100/dashboard and take a screenshot"

Step 2: Verify Data
"Extract device status from the dashboard and verify all devices are online"

Step 3: Test Form
"Fill the WiFi configuration form and click Submit"

Step 4: Validate
"Verify success message appears and new WiFi SSID is displayed"
```

⏱️ **Total Time:** ~5 minutes (vs 20-30 minutes manually)

---

## 📊 Model Selection Quick Guide

| Task Type | Model to Use | Example |
|-----------|--------------|---------|
| Quick builds | `haiku` | "Build the project" |
| Feature development | `sonnet` (default) | "Add new BLE command" |
| Complex debugging | `opus` | "Debug race condition in tasks" |
| Architecture design | `opusplan` | "Design OTA update system" |

**Usage:**
```bash
# Use Haiku
claude --model haiku "Build project"

# Use Sonnet (default)
claude "Add BLE notification support"

# Use Opus
claude --model opus "Design firmware architecture"

# Use OpusPlan
claude --model opusplan "Plan and implement OTA updates"
```

---

## 💡 Pro Tips

### 1. Combine MCPs for Maximum Power

```
"Using latest ESP32 documentation (use context7),
implement MQTT reconnection, build firmware,
and test with Playwright on broker web UI"
```

### 2. Use --continue for Long Sessions

```bash
# Start
claude "Start implementing OTA feature"

# Continue later (reuses context)
claude --continue "Now test OTA update"
```

### 3. Batch Commands

```bash
# Instead of 3 separate calls:
claude "Build project"
claude "Upload firmware"
claude "Monitor logs"

# Do in one:
claude "Build, upload, and monitor logs for 60 seconds"
```

### 4. Leverage Context7 for Versions

```
# Always specify version for accuracy
"Show me ArduinoJson 7.4.2 API. use context7"  # ✅ Good
"Show me ArduinoJson API. use context7"        # ❌ Might get wrong version
```

---

## 🐛 Quick Troubleshooting

### MCP Not Connected

```bash
# Fix: Restart MCP server
claude mcp remove <name>
claude mcp add <name> -- <command>
claude mcp list
```

### Arduino CLI Not Found

```bash
# Install Arduino CLI
winget install ArduinoSA.CLI

# Verify
arduino-cli version
```

### Playwright Browser Issues

```bash
# Install browsers
npx playwright install
```

---

## 📚 Learn More

- **Full Guide:** `MCP_GATEWAY_GUIDE.md` - Comprehensive documentation
- **CLAUDE.md:** AI assistant reference
- **API.md:** BLE API documentation

---

## 🎉 You're Ready!

Now you can:
- ✅ Build firmware with one command
- ✅ Get latest documentation instantly
- ✅ Automate web testing
- ✅ Develop 4x faster

**Start developing:**
```bash
claude "Help me build the Gateway firmware"
```

**Happy coding! 🚀**

---

**SURIOTA R&D Team | Powered by Claude Code + MCP**
