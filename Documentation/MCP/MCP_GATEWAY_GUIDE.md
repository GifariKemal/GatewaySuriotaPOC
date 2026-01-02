# MCP Gateway Development Guide

**Panduan Lengkap Penggunaan MCP Servers untuk Pengembangan SRT-MGATE-1210
Gateway**

**Version:** 1.0.0 **Last Updated:** November 24, 2025 **Project:**
SRT-MGATE-1210 Industrial IoT Gateway **Developer:** SURIOTA R&D Team

---

## 📋 Daftar Isi

1. [Pengenalan MCP (Model Context Protocol)](#pengenalan-mcp)
2. [MCP Servers yang Terinstall](#mcp-servers-yang-terinstall)
3. [Context7 MCP - Up-to-date Documentation](#context7-mcp)
4. [Playwright MCP - Browser Automation](#playwright-mcp)
5. [Arduino MCP - Build Automation](#arduino-mcp)
6. [Workflow Pengembangan Gateway](#workflow-pengembangan-gateway)
7. [Claude CLI & Model Selection](#claude-cli--model-selection)
8. [Production Readiness Checklist](#production-readiness-checklist)
9. [Best Practices](#best-practices)
10. [Troubleshooting](#troubleshooting)

---

## 🎯 Pengenalan MCP

### Apa itu MCP (Model Context Protocol)?

**Model Context Protocol (MCP)** adalah protokol standar terbuka yang
memungkinkan Large Language Models (LLMs) seperti Claude untuk berinteraksi
dengan tools eksternal, data sources, dan services.

### Mengapa MCP Penting untuk Gateway Development?

| Masalah Tradisional                      | Solusi dengan MCP                      |
| ---------------------------------------- | -------------------------------------- |
| Manual build & upload firmware           | ✅ Automated build dengan Arduino MCP  |
| Dokumentasi library outdated             | ✅ Real-time docs dengan Context7      |
| Manual testing web dashboard             | ✅ Automated testing dengan Playwright |
| Context switching (IDE ↔ Browser ↔ Docs) | ✅ Everything in one place (Claude)    |
| Knowledge cutoff limitation              | ✅ Up-to-date information              |

### Arsitektur MCP untuk Gateway Suriota

```
┌─────────────────────────────────────────────────────┐
│                  CLAUDE CODE                        │
│         (Your AI Development Assistant)             │
└────────────────┬────────────────────────────────────┘
                 │ MCP Protocol
                 │
        ┌────────┴────────┐
        │                 │
   ┌────▼─────┐    ┌─────▼─────┐    ┌──────▼──────┐
   │ Arduino  │    │ Context7  │    │  Playwright │
   │   MCP    │    │    MCP    │    │     MCP     │
   └────┬─────┘    └─────┬─────┘    └──────┬──────┘
        │                │                  │
   ┌────▼─────┐    ┌─────▼─────┐    ┌──────▼──────┐
   │ arduino- │    │ Official  │    │   Browser   │
   │   cli    │    │   Docs    │    │  Automation │
   └────┬─────┘    └─────┬─────┘    └──────┬──────┘
        │                │                  │
   ┌────▼─────┐    ┌─────▼─────┐    ┌──────▼──────┐
   │ ESP32-S3 │    │ Libraries │    │ Web Testing │
   │ Gateway  │    │ Framework │    │  Dashboard  │
   └──────────┘    └───────────┘    └─────────────┘
```

---

## 🔌 MCP Servers yang Terinstall

### Overview MCP Servers

Untuk project Gateway Suriota, kita menggunakan **3 MCP servers**:

| MCP Server      | Fungsi Utama                    | Use Case Gateway                    |
| --------------- | ------------------------------- | ----------------------------------- |
| **Arduino MCP** | Build, upload, monitor firmware | Automate ESP32 development workflow |
| **Context7**    | Real-time documentation         | Get latest ESP32/Arduino docs       |
| **Playwright**  | Browser automation              | Test web dashboard & MQTT broker UI |

### Melihat Status MCP Servers

```bash
# List semua MCP servers
claude mcp list

# Output:
# context7: npx -y @upstash/context7-mcp - ✓ Connected
# playwright: npx @playwright/mcp@latest - ✓ Connected
```

### Konfigurasi MCP (`.claude.json`)

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
    },
    "context7": {
      "type": "stdio",
      "command": "npx",
      "args": ["-y", "@upstash/context7-mcp"],
      "env": {}
    },
    "playwright": {
      "type": "stdio",
      "command": "npx",
      "args": ["@playwright/mcp@latest"],
      "env": {}
    }
  }
}
```

---

## 📚 Context7 MCP - Up-to-date Documentation

### Apa itu Context7?

**Context7** adalah MCP server yang menyediakan **dokumentasi up-to-date** untuk
libraries dan frameworks. Mengatasi masalah LLM yang training data-nya sudah
outdated.

### Fitur Context7

✅ **Real-time Documentation** - Fetch dokumentasi terbaru dari source ✅
**Version-Specific** - Dokumentasi sesuai versi library yang Anda gunakan ✅
**Code Examples** - Contoh kode yang accurate dan tested ✅ **Multi-Language** -
Support berbagai bahasa pemrograman ✅ **Framework Coverage** - ESP32, Arduino,
React, Next.js, dll

### Cara Menggunakan Context7

#### Basic Syntax

Tambahkan **"use context7"** di akhir prompt:

```
[Your question about library/framework]
use context7
```

#### Contoh Penggunaan untuk Gateway

**1. ESP32 Arduino Core Documentation**

```
How do I optimize PSRAM allocation for large JSON documents on ESP32-S3?
What are the best practices for heap_caps_malloc vs malloc?
use context7
```

**Output:**

- Latest ESP32 Arduino Core documentation
- PSRAM allocation best practices
- Code examples dengan MALLOC_CAP_SPIRAM
- Performance benchmarks

**2. ArduinoJson Library (Current Version)**

```
Show me the latest ArduinoJson 7.x API for:
- Parsing streaming JSON from MQTT
- Using PSRAM for large documents
- Handling nested objects efficiently
use context7
```

**Output:**

- ArduinoJson 7.x specific APIs
- SpiRamJsonDocument usage
- Memory optimization techniques
- Deserialization strategies

**3. FreeRTOS on ESP32**

```
What are the best practices for task synchronization in ESP32 FreeRTOS when:
- Multiple tasks access shared config (ConfigManager)
- Task priorities need balancing (MQTT vs Modbus)
- Preventing priority inversion
use context7
```

**Output:**

- FreeRTOS semaphore patterns
- Task priority guidelines
- Mutex vs binary semaphore
- Real-world ESP32 examples

**4. MQTT PubSubClient**

```
How do I implement MQTT QoS 2 with PubSubClient library on ESP32?
Include reconnection logic and persistent queue handling.
use context7
```

**Output:**

- PubSubClient latest API
- QoS implementation details
- Reconnection strategies
- Queue management patterns

**5. Modbus Library Documentation**

```
Show me the latest ModbusMaster library API for:
- Handling Modbus exceptions
- Timeout configuration
- Multiple serial ports (RS485)
use context7
```

**Output:**

- ModbusMaster current API
- Exception handling codes
- Timeout best practices
- Multi-port management

### Korelasi dengan Project Gateway

Context7 membantu development Gateway dengan:

#### 1. **Mengatasi Knowledge Cutoff**

**Masalah:**

- Claude training data: January 2025
- ESP32 Arduino Core terus update
- ArduinoJson 7.x baru release
- API changes di library dependencies

**Solusi:**

```
use context7
```

→ Fetch dokumentasi **terbaru** dari official source

#### 2. **Mengurangi Trial & Error**

**Tanpa Context7:**

```
Developer: "How to use ArduinoJson?"
Claude: [Provides ArduinoJson 6.x API - outdated]
Developer: [Code fails - API changed in v7]
Developer: [Googles documentation]
Developer: [Rewrites code with correct API]
```

**Dengan Context7:**

```
Developer: "How to use ArduinoJson 7.x? use context7"
Claude: [Fetches ArduinoJson 7.x docs]
Claude: [Provides correct v7 API + examples]
Developer: [Code works first time]
```

#### 3. **Version-Specific Solutions**

**Scenario:** Gateway menggunakan library-specific versions

```json
// dependencies.txt
ArduinoJson @ 7.4.2
RTClib @ 2.1.4
ModbusMaster @ 2.0.1
```

**Query dengan Context7:**

```
Using ArduinoJson 7.4.2, show me how to handle 100KB JSON config files
with PSRAM on ESP32-S3 with memory recovery.
use context7
```

→ Context7 fetch dokumentasi **exact version** yang digunakan

---

## 🎭 Playwright MCP - Browser Automation

### Apa itu Playwright MCP?

**Playwright MCP** adalah MCP server dari **Microsoft** yang menyediakan
kemampuan **browser automation** menggunakan Playwright. Claude bisa interact
dengan web pages seperti manusia.

### Fitur Playwright MCP

✅ **Navigate Web Pages** - Buka URL, klik link, fill form ✅ **Extract Data** -
Scrape content, parse tables, extract metrics ✅ **Take Screenshots** - Visual
verification, bug reporting ✅ **Test Web Apps** - Automated UI testing,
regression testing ✅ **Execute JavaScript** - Run custom scripts di browser ✅
**Wait for Elements** - Smart waiting for dynamic content ✅ **Multi-Browser** -
Chromium, Firefox, WebKit support

### Teknologi Playwright

**Keunggulan dibanding scraping tradisional:**

| Traditional Scraping  | Playwright MCP              |
| --------------------- | --------------------------- |
| HTML parsing only     | ✅ Full browser execution   |
| No JavaScript support | ✅ JavaScript execution     |
| No dynamic content    | ✅ Waits for dynamic loads  |
| Brittle selectors     | ✅ Accessibility tree-based |
| Manual screenshots    | ✅ Automated visual testing |

### Cara Menggunakan Playwright MCP

#### Basic Operations

**1. Navigate & Screenshot**

```
Open https://suriota.com and take a screenshot of the homepage
```

**2. Extract Information**

```
Go to https://github.com/espressif/arduino-esp32 and tell me:
- Current version
- Number of stars
- Latest release date
- Open issues count
```

**3. Form Interaction**

```
Navigate to https://example.com/login and:
1. Fill username field with "admin"
2. Fill password field with "password123"
3. Click the login button
4. Verify redirect to dashboard
```

**4. Web Scraping**

```
Go to https://arduino.cc/reference/en/ and extract:
- All function names in the Serial library
- Their descriptions
- Example code snippets
```

### Korelasi dengan Project Gateway

Playwright MCP critical untuk **testing dan monitoring** Gateway:

#### 1. **Test MQTT Broker Web Interface**

**Scenario:** Gateway publish data ke HiveMQ broker

```
Open http://broker.hivemq.com:8080 and verify:
- Topic "suriota/gateway/data" exists
- Last message timestamp is recent (< 1 minute ago)
- Message payload contains valid JSON
- Connection count shows our gateway
```

**Use Case:**

- Verify MQTT publishing works
- Debug message format issues
- Monitor broker health
- Validate QoS settings

#### 2. **Test Gateway Web Dashboard**

**Scenario:** Gateway has HTTP API at `http://192.168.1.100`

```
Test our gateway status dashboard:
1. Navigate to http://192.168.1.100/status
2. Verify "Network Status" shows "Connected"
3. Check "MQTT Status" is "Publishing"
4. Extract "Uptime" value
5. Screenshot the dashboard
6. Verify no error messages displayed
```

**Use Case:**

- Automated dashboard testing
- Regression testing after firmware updates
- Visual verification (screenshots)
- Performance monitoring

#### 3. **Monitor Modbus Simulator Web UI**

**Scenario:** Testing dengan Modbus TCP web simulator

```
Open http://localhost:8080/modbus-simulator and:
1. Configure slave ID = 1
2. Set register 0 = 100 (temperature)
3. Set register 1 = 75 (humidity)
4. Verify values are set correctly
5. Test read holding registers function
```

**Use Case:**

- Setup test environment
- Verify simulator configuration
- Automated test data injection

#### 4. **Scrape ESP32 Documentation**

```
Go to https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/ and extract:
- PSRAM configuration options
- Available memory sizes
- Performance benchmarks
- Code examples for heap_caps_malloc
```

**Use Case:**

- Research optimization techniques
- Get latest API documentation
- Find code examples
- Performance tuning reference

#### 5. **Test BLE Configuration Mobile App**

**Scenario:** Gateway memiliki BLE configuration web app

```
Test BLE configuration web interface at http://localhost:3000:
1. Open the app in mobile viewport (375x667)
2. Click "Scan for Devices"
3. Verify gateway appears with name "SURIOTA GW"
4. Click connect
5. Read current WiFi SSID
6. Update WiFi credentials
7. Verify success message
8. Take screenshot of configuration page
```

**Use Case:**

- Mobile app testing
- BLE integration verification
- UI/UX validation
- Cross-browser compatibility

---

## 🔧 Arduino MCP - Build Automation

### Apa itu Arduino MCP?

**Arduino MCP** adalah custom MCP server yang dibuat khusus untuk project
Gateway Suriota. Menyediakan automation untuk Arduino CLI operations.

### Fitur Arduino MCP

✅ **Build Firmware** - Compile ESP32 projects ✅ **Upload to Board** - Flash
firmware ke ESP32-S3 ✅ **Serial Monitor** - Real-time log monitoring ✅
**Library Management** - Install/update Arduino libraries ✅ **Board
Detection** - Auto-detect connected boards ✅ **Error Analysis** - Parse dan
suggest fixes untuk compilation errors ✅ **Clean Builds** - Remove build
artifacts

### Cara Menggunakan Arduino MCP

#### 1. Build Firmware

```
Build the Main project at C:/Users/Administrator/Music/GatewaySuriotaPOC/Main
```

**Output:**

- Compilation status
- Binary size
- Memory usage (Flash/SRAM)
- Warnings/errors

#### 2. Upload Firmware

```
Upload the firmware to the connected ESP32-S3 board
```

**Output:**

- Auto-detect COM port
- Upload progress
- Verification status
- Success/failure message

#### 3. Monitor Serial Output

```
Monitor COM3 at 115200 baud for 30 seconds and show me the boot logs
```

**Output:**

- Real-time serial logs
- System initialization messages
- Error messages (if any)

#### 4. Install Library

```
Install ArduinoJson library version 7.4.2
```

**Output:**

- Download progress
- Installation status
- Dependencies (if any)

### Korelasi dengan Project Gateway

Arduino MCP **automate** seluruh development workflow:

#### Complete Development Cycle

```
I need to add a new Modbus device with calibration support.

Step 1: Get latest documentation
"Show me ModbusMaster library API for reading holding registers. use context7"

Step 2: Write code
[Claude generates code based on latest docs]

Step 3: Build
"Build the Main project"

Step 4: Fix errors (if any)
"The build failed. Analyze errors and suggest fixes"

Step 5: Upload
"Upload the firmware to ESP32-S3"

Step 6: Monitor
"Monitor serial output for 60 seconds and verify the new device polls correctly"

Step 7: Test with Playwright
"Open Modbus simulator and verify register values match expected data"
```

**All done in ONE conversation with Claude!** 🎉

---

## 🚀 Workflow Pengembangan Gateway

### Workflow 1: Adding New Feature

**Goal:** Tambah fitur device enable/disable via BLE

```
┌─────────────────────────────────────────────────┐
│ Step 1: Research API Documentation              │
└─────────────────────────────────────────────────┘

Prompt:
"Show me the latest ArduinoJson 7.x API for adding boolean fields
to existing JSON objects. use context7"

Output:
→ Latest JsonDocument API
→ Code examples for adding fields
→ Memory considerations

┌─────────────────────────────────────────────────┐
│ Step 2: Design & Implement                      │
└─────────────────────────────────────────────────┘

Prompt:
"Add a boolean 'enabled' field to device configuration in Main/ConfigManager.cpp
Follow existing code patterns and update CRUD operations"

Output:
→ Claude reads existing code
→ Implements feature
→ Updates CRUDHandler
→ Adds validation

┌─────────────────────────────────────────────────┐
│ Step 3: Build & Test                            │
└─────────────────────────────────────────────────┘

Prompt:
"Build the project and upload to ESP32-S3"

Output:
→ Arduino MCP compiles
→ Reports any errors
→ Uploads firmware
→ Returns success status

┌─────────────────────────────────────────────────┐
│ Step 4: Monitor & Verify                        │
└─────────────────────────────────────────────────┘

Prompt:
"Monitor serial output for 60 seconds and verify:
- ConfigManager loads enabled field
- CRUDHandler accepts boolean in JSON
- ModbusRtuService respects enabled flag"

Output:
→ Arduino MCP captures logs
→ Shows boot sequence
→ Verifies feature works

┌─────────────────────────────────────────────────┐
│ Step 5: Document                                 │
└─────────────────────────────────────────────────┘

Prompt:
"Update Documentation/API_Reference/BLE_DEVICE_CONTROL.md
with the new 'enabled' field API"

Output:
→ Claude updates documentation
→ Adds code examples
→ Updates version history
```

**Time Saved:** 2-3 hours → 30 minutes ⚡

### Workflow 2: Debugging Production Issue

**Scenario:** Gateway stops publishing to MQTT after 24 hours

```
┌─────────────────────────────────────────────────┐
│ Step 1: Get Logs                                 │
└─────────────────────────────────────────────────┘

Prompt:
"Upload debug firmware with verbose MQTT logging and monitor for 5 minutes"

Output:
→ Build with LOG_DEBUG
→ Upload to board
→ Capture detailed logs

┌─────────────────────────────────────────────────┐
│ Step 2: Analyze with Documentation               │
└─────────────────────────────────────────────────┘

Prompt:
"The logs show 'MQTT publish failed: -2' after 24 hours.
What does error code -2 mean in PubSubClient library?
use context7"

Output:
→ Fetches PubSubClient docs
→ Error code -2 = MQTT_CONNECTION_LOST
→ Suggests reconnection logic

┌─────────────────────────────────────────────────┐
│ Step 3: Research Best Practices                 │
└─────────────────────────────────────────────────┘

Prompt:
"Show me best practices for MQTT keep-alive and reconnection
in PubSubClient on ESP32 for long-running applications.
use context7"

Output:
→ Keep-alive configuration
→ Reconnection strategies
→ Network monitoring patterns

┌─────────────────────────────────────────────────┐
│ Step 4: Implement Fix                            │
└─────────────────────────────────────────────────┘

Prompt:
"Implement automatic MQTT reconnection in Main/MqttManager.cpp:
- Check connection every 5s
- Reconnect with exponential backoff
- Log reconnection attempts"

Output:
→ Claude implements fix
→ Follows existing patterns
→ Adds error handling

┌─────────────────────────────────────────────────┐
│ Step 5: Test Fix                                 │
└─────────────────────────────────────────────────┘

Prompt:
"Build and upload firmware, then:
1. Monitor for successful MQTT connection
2. Use Playwright to check broker web UI
3. Verify messages are publishing"

Output:
→ Build & upload successful
→ Serial shows reconnection working
→ Playwright confirms broker receiving messages

┌─────────────────────────────────────────────────┐
│ Step 6: Regression Test                          │
└─────────────────────────────────────────────────┘

Prompt:
"Create a test that simulates network disconnection:
1. Disconnect WiFi
2. Wait 30s
3. Reconnect WiFi
4. Verify MQTT reconnects within 10s"

Output:
→ Test plan created
→ Playwright automates testing
→ Results logged
```

**Result:** Bug fixed, tested, and prevented from recurring! 🐛→✅

### Workflow 3: Performance Optimization

**Goal:** Optimize memory usage untuk support 100+ Modbus devices

```
┌─────────────────────────────────────────────────┐
│ Step 1: Research PSRAM Best Practices            │
└─────────────────────────────────────────────────┘

Prompt:
"What are the best practices for PSRAM allocation on ESP32-S3
for large configuration files (>100KB)?
Include heap_caps_malloc usage and memory recovery.
use context7"

Output:
→ Latest ESP32 Arduino Core docs
→ PSRAM allocation patterns
→ Memory recovery strategies
→ Performance benchmarks

┌─────────────────────────────────────────────────┐
│ Step 2: Analyze Current Memory Usage             │
└─────────────────────────────────────────────────┘

Prompt:
"Build firmware with memory profiling and show:
- DRAM free at startup
- PSRAM free at startup
- Peak memory usage during device polling
- Largest memory allocations"

Output:
→ Memory report
→ Identifies memory hogs
→ Suggests optimizations

┌─────────────────────────────────────────────────┐
│ Step 3: Implement Optimizations                  │
└─────────────────────────────────────────────────┘

Prompt:
"Optimize ConfigManager to use PSRAM:
1. Allocate JsonDocument in PSRAM
2. Move device cache to PSRAM
3. Implement cache size limits
4. Add memory monitoring"

Output:
→ Code refactored
→ PSRAM allocation added
→ Memory limits enforced

┌─────────────────────────────────────────────────┐
│ Step 4: Benchmark                                 │
└─────────────────────────────────────────────────┘

Prompt:
"Build optimized firmware and test with 100 Modbus devices:
1. Monitor memory usage
2. Measure polling performance
3. Verify no memory leaks after 1 hour
4. Compare with previous version"

Output:
→ Performance metrics
→ Memory usage graphs
→ Before/after comparison
→ Leak detection results
```

**Result:** Support 50 devices → 100+ devices! 📈

---

## 🎓 Claude CLI & Model Selection

### Claude CLI Commands

#### Basic Commands

```bash
# Interactive mode (default)
claude

# Print mode (single response)
claude -p "Build the Main project"

# Continue previous conversation
claude --continue

# Resume specific session
claude --resume

# Run with specific model
claude --model opus
claude --model sonnet
claude --model haiku

# Thinking mode
claude "think about how to optimize PSRAM usage"

# Multiple commands in queue
claude
> Build project
[Press Enter while Claude works]
> Also run tests
```

#### MCP Commands

```bash
# List MCP servers
claude mcp list

# Add MCP server
claude mcp add <name> -- <command>

# Remove MCP server
claude mcp remove <name>

# Test MCP server
claude mcp add-from-claude-desktop
```

#### Configuration Commands

```bash
# Edit permissions
claude /permissions

# View/edit memory
claude /memory

# Change model
claude /model

# View usage
claude /usage

# View cost
claude /cost
```

### Model Selection Guide

#### Model Comparison

| Model          | Speed       | Cost      | Best For                 | Gateway Use Case               |
| -------------- | ----------- | --------- | ------------------------ | ------------------------------ |
| **Haiku 4.5**  | ⚡⚡⚡ Fast | $ Low     | Quick tasks, code review | Monitor logs, quick fixes      |
| **Sonnet 4.5** | ⚡⚡ Medium | $$ Medium | Balanced performance     | Most development tasks         |
| **Opus 4**     | ⚡ Slower   | $$$ High  | Complex reasoning        | Architecture design, debugging |

#### When to Use Each Model

**Haiku 4.5** - Quick Operations

```bash
# Use Haiku for fast, simple tasks
claude --model haiku

Examples:
- "Monitor serial output for 30 seconds"
- "List all connected boards"
- "Build the project"
- "Install ArduinoJson library"
- "Take screenshot of dashboard"
```

**Cost:** ~$0.25 per million input tokens **Speed:** ~50 tokens/second **Sweet
Spot:** Repetitive tasks, quick queries

**Sonnet 4.5** - Development Workhorse (DEFAULT)

```bash
# Use Sonnet for most development (default)
claude

Examples:
- "Add new BLE command for device control"
- "Optimize memory usage in ConfigManager"
- "Fix compilation errors"
- "Update documentation"
- "Implement MQTT reconnection logic"
```

**Cost:** ~$3 per million input tokens **Speed:** ~30 tokens/second **Sweet
Spot:** 90% of development tasks

**Opus 4** - Complex Problem Solving

```bash
# Use Opus for complex reasoning
claude --model opus

Examples:
- "Design architecture for firmware update over MQTT"
- "Debug race condition in FreeRTOS tasks"
- "Optimize entire system for production deployment"
- "Analyze memory leak across multiple modules"
- "Design security strategy for BLE authentication"
```

**Cost:** ~$15 per million input tokens **Speed:** ~15 tokens/second **Sweet
Spot:** Critical design decisions, complex debugging

#### Plan Mode (OpusPlan / SonnetPlan)

**OpusPlan** - Best for Complex Planning

```bash
# OpusPlan: Opus for planning, Sonnet for execution
claude --model opusplan

Prompt:
"Plan and implement a complete firmware update system over MQTT
with version checking, rollback support, and progress reporting"

Behavior:
→ Planning Phase: Uses Opus (deep thinking)
→ Execution Phase: Uses Sonnet (fast implementation)
```

**SonnetPlan** - Balanced Planning (DEFAULT for "think" keyword)

```bash
# SonnetPlan: Sonnet for planning and execution
claude

Prompt:
"Think about how to add OTA firmware update support"

Behavior:
→ Planning Phase: Uses Sonnet with extended thinking
→ Execution Phase: Uses Sonnet
```

### Recommended Model Strategy for Gateway

```
┌─────────────────────────────────────────────┐
│         Development Phase                   │
├─────────────────────────────────────────────┤
│ Quick fixes & builds  →  Haiku              │
│ Feature development   →  Sonnet (default)   │
│ Architecture design   →  Opus / OpusPlan    │
│ Complex debugging     →  Opus               │
│ Documentation         →  Sonnet             │
│ Code review           →  Haiku / Sonnet     │
└─────────────────────────────────────────────┘
```

### Cost Optimization Tips

**1. Use Plan Mode Wisely**

```bash
# ❌ Expensive: OpusPlan for simple task
claude --model opusplan "Build the project"

# ✅ Efficient: Haiku for simple task
claude --model haiku "Build the project"
```

**2. Batch Related Tasks**

```bash
# ❌ Multiple sessions (more cost)
claude -p "Build project"
claude -p "Upload firmware"
claude -p "Monitor logs"

# ✅ Single session (less cost)
claude -p "Build project, upload firmware, and monitor logs for 30 seconds"
```

**3. Use --continue for Long Tasks**

```bash
# Start session
claude "Start implementing OTA update feature"

# Continue later (reuses context)
claude --continue "Now test the OTA update"
```

---

## ✅ Production Readiness Checklist

### Phase 1: Core Functionality ✅

- [x] Modbus RTU support (2x RS485 ports)
- [x] Modbus TCP support
- [x] BLE configuration interface
- [x] MQTT publishing
- [x] HTTP API support
- [x] SD card logging
- [x] RTC timestamps
- [x] Network failover (WiFi ↔ Ethernet)

### Phase 2: Reliability & Robustness 🚧

#### Memory Management

- [x] PSRAM allocation with fallback
- [x] Automatic memory recovery
- [x] Memory leak detection
- [ ] **TODO:** Long-term stability test (7 days continuous)
- [ ] **TODO:** Memory usage optimization for 100+ devices

#### Error Handling

- [x] Unified error codes
- [x] Error reporting system
- [x] Graceful degradation
- [ ] **TODO:** Comprehensive error recovery testing
- [ ] **TODO:** Error logging to SD card

#### Network Reliability

- [x] Network failover with hysteresis
- [x] MQTT reconnection logic
- [ ] **TODO:** Network stress testing
- [ ] **TODO:** Offline queue persistence
- [ ] **TODO:** Bandwidth optimization

### Phase 3: Performance Optimization 🚧

#### Modbus Performance

- [ ] **TODO:** Polling optimization (parallel vs sequential)
- [ ] **TODO:** Timeout tuning for production environment
- [ ] **TODO:** Baudrate switching optimization
- [ ] **TODO:** Response time benchmarking

#### MQTT Performance

- [ ] **TODO:** Queue size optimization
- [ ] **TODO:** Batch publishing implementation
- [ ] **TODO:** Compression for large payloads
- [ ] **TODO:** QoS strategy for production

#### Memory Optimization

- [ ] **TODO:** JSON document size optimization
- [ ] **TODO:** Cache eviction policy
- [ ] **TODO:** String pooling implementation
- [ ] **TODO:** Stack size optimization per task

### Phase 4: Security 🔒

#### Authentication & Authorization

- [ ] **TODO:** BLE pairing with PIN code
- [ ] **TODO:** MQTT authentication (username/password)
- [ ] **TODO:** HTTP API authentication
- [ ] **TODO:** Role-based access control

#### Data Security

- [ ] **TODO:** TLS/SSL for MQTT
- [ ] **TODO:** HTTPS for HTTP API
- [ ] **TODO:** Encrypted configuration storage
- [ ] **TODO:** Secure boot implementation

#### Vulnerability Assessment

- [ ] **TODO:** Input validation (BLE, MQTT, HTTP)
- [ ] **TODO:** Buffer overflow protection
- [ ] **TODO:** SQL injection prevention (if using database)
- [ ] **TODO:** XSS protection (web interface)

### Phase 5: Testing & Validation 🧪

#### Unit Testing

- [ ] **TODO:** ConfigManager unit tests
- [ ] **TODO:** CRUDHandler unit tests
- [ ] **TODO:** QueueManager unit tests
- [ ] **TODO:** ModbusRtuService unit tests

#### Integration Testing

- [ ] **TODO:** End-to-end Modbus → MQTT flow
- [ ] **TODO:** BLE configuration → Device polling
- [ ] **TODO:** Network failover scenarios
- [ ] **TODO:** Firmware update process

#### Stress Testing

- [ ] **TODO:** 100 devices continuous polling (24 hours)
- [ ] **TODO:** Network disconnect/reconnect cycles
- [ ] **TODO:** Memory stress test
- [ ] **TODO:** CPU load testing

#### Field Testing

- [ ] **TODO:** Factory environment testing (temperature, humidity)
- [ ] **TODO:** EMI/EMC compliance testing
- [ ] **TODO:** Power stability testing
- [ ] **TODO:** Long-term reliability (30 days)

### Phase 6: Documentation 📚

#### Technical Documentation

- [x] CLAUDE.md - AI assistant guide
- [x] API.md - BLE API reference
- [x] MODBUS_DATATYPES.md - Data types reference
- [ ] **TODO:** DEPLOYMENT_GUIDE.md
- [ ] **TODO:** TROUBLESHOOTING_ADVANCED.md
- [ ] **TODO:** SECURITY_GUIDELINES.md

#### User Documentation

- [ ] **TODO:** USER_MANUAL.md
- [ ] **TODO:** QUICK_START_GUIDE.md
- [ ] **TODO:** CONFIGURATION_GUIDE.md
- [ ] **TODO:** FAQ.md

#### Developer Documentation

- [ ] **TODO:** CONTRIBUTING.md
- [ ] **TODO:** CODING_STANDARDS.md
- [ ] **TODO:** TESTING_GUIDE.md
- [ ] **TODO:** RELEASE_PROCESS.md

### Phase 7: Production Deployment 🚀

#### Pre-Deployment

- [ ] **TODO:** Create production firmware build (PRODUCTION_MODE = 1)
- [ ] **TODO:** Factory reset procedure
- [ ] **TODO:** Default configuration template
- [ ] **TODO:** Calibration procedure

#### Deployment Tools

- [ ] **TODO:** Firmware flashing script
- [ ] **TODO:** Configuration backup/restore tool
- [ ] **TODO:** Remote monitoring dashboard
- [ ] **TODO:** Diagnostic tool

#### Post-Deployment

- [ ] **TODO:** Remote logging (syslog/cloud)
- [ ] **TODO:** Firmware update over MQTT
- [ ] **TODO:** Performance monitoring
- [ ] **TODO:** Usage analytics

### Phase 8: Commercialization 💰

#### Compliance & Certification

- [ ] **TODO:** CE marking (if applicable)
- [ ] **TODO:** FCC certification (if applicable)
- [ ] **TODO:** RoHS compliance
- [ ] **TODO:** Industry-specific certifications

#### Marketing Materials

- [ ] **TODO:** Product datasheet
- [ ] **TODO:** Technical specifications
- [ ] **TODO:** Application notes
- [ ] **TODO:** Demo videos

#### Support Infrastructure

- [ ] **TODO:** Customer support portal
- [ ] **TODO:** Ticketing system
- [ ] **TODO:** Knowledge base
- [ ] **TODO:** Training materials

---

## 💡 Best Practices

### 1. Effective Prompting with MCP

#### ✅ Good Prompts

```
"Using the latest ArduinoJson 7.x documentation (use context7),
optimize ConfigManager to handle 100 devices with PSRAM allocation.
Then build and test on ESP32-S3."
```

**Why Good:**

- Specifies exact version (ArduinoJson 7.x)
- Invokes Context7 explicitly
- Clear goal (100 devices)
- Includes testing step

#### ❌ Bad Prompts

```
"Make it faster"
```

**Why Bad:**

- Vague goal
- No context
- No testing criteria
- No MCP invocation

### 2. Combining Multiple MCPs

**Workflow Example:**

```
Step 1: Research (Context7)
"Show me ESP32 FreeRTOS task synchronization patterns. use context7"

Step 2: Implement (Arduino MCP)
"Implement mutex protection for ConfigManager cache based on the patterns above.
Build the project."

Step 3: Test (Playwright MCP)
"Open Modbus simulator and configure 10 concurrent devices.
Verify no race conditions in config access."
```

### 3. Model Selection Strategy

```
┌─────────────────────────────────────────────┐
│ Task Complexity Analysis                    │
├─────────────────────────────────────────────┤
│ Lines of code to write: < 50    → Haiku    │
│ Lines of code to write: 50-200  → Sonnet   │
│ Lines of code to write: > 200   → Opus     │
│                                             │
│ Requires deep reasoning?        → Opus     │
│ Quick iteration needed?         → Haiku    │
│ Balanced approach?              → Sonnet   │
└─────────────────────────────────────────────┘
```

### 4. Version Control Integration

```bash
# Before major changes
git checkout -b feature/device-enable-disable

# Develop with Claude
claude "Implement device enable/disable feature"

# Review changes
claude --model haiku "Review the changes I made and suggest improvements"

# Commit
git add .
git commit -m "Add device enable/disable feature

✅ Implemented 'enabled' boolean field
✅ Updated CRUD operations
✅ Added validation
✅ Updated documentation

🤖 Generated with Claude Code
Co-Authored-By: Claude <noreply@anthropic.com>"

# Create PR
gh pr create --title "Add device enable/disable feature"
```

### 5. Documentation as You Code

```
# Good practice: Update docs immediately
claude "Add new feature AND update Documentation/API_Reference/API.md"

# Bad practice: Defer documentation
claude "Add new feature"
[Later...] "Uhh, what did this feature do again?"
```

---

## 🐛 Troubleshooting

### MCP Server Not Responding

**Symptom:**

```
Error: MCP server 'context7' not responding
```

**Solution:**

```bash
# Check MCP server status
claude mcp list

# Restart MCP server
claude mcp remove context7
claude mcp add context7 -- npx -y @upstash/context7-mcp

# Verify connection
claude mcp list
```

### Playwright Browser Not Launching

**Symptom:**

```
Error: Failed to launch browser
```

**Solution:**

```bash
# Install Playwright browsers
npx playwright install

# Or reinstall Playwright MCP
claude mcp remove playwright
claude mcp add playwright -- npx @playwright/mcp@latest
```

### Arduino MCP Build Failures

**Symptom:**

```
Error: arduino-cli not found
```

**Solution:**

```bash
# Check arduino-cli installation
arduino-cli version

# If not installed, install it
winget install ArduinoSA.CLI

# Verify in PATH
where arduino-cli
```

### Context7 Rate Limiting

**Symptom:**

```
Error: Rate limit exceeded
```

**Solution:**

```bash
# Get API key from https://context7.com/dashboard
# Then update configuration
claude mcp remove context7
claude mcp add context7 -- npx -y @upstash/context7-mcp --api-key YOUR_API_KEY
```

### High Claude Usage Costs

**Symptom:**

- Monthly bill too high

**Solution:**

1. Use Haiku for simple tasks
2. Batch related tasks in one session
3. Use --continue to reuse context
4. Avoid OpusPlan for trivial tasks
5. Monitor usage with `claude /usage`

---

## 📊 Success Metrics

### Development Velocity

**Before MCP:**

- Feature implementation: 4-8 hours
- Bug fixing: 2-4 hours
- Documentation: 1-2 hours
- **Total:** 7-14 hours per feature

**After MCP:**

- Feature implementation: 1-2 hours (Context7 + Arduino MCP)
- Bug fixing: 30 minutes - 1 hour (Playwright + logs)
- Documentation: 15-30 minutes (automated)
- **Total:** 2-3.5 hours per feature

**Productivity Gain:** **~4x faster** 🚀

### Code Quality

**Metrics:**

- ✅ Up-to-date APIs (Context7)
- ✅ Fewer compilation errors
- ✅ Better error handling
- ✅ Comprehensive documentation
- ✅ Automated testing (Playwright)

### Time to Production

**Target:**

- Current: Phase 2 (Reliability) - ~60% complete
- Goal: Production-ready firmware in **2-3 months**
- Commercialization: **Q2 2026**

---

## 🎯 Next Steps

### Week 1-2: Complete Phase 2 (Reliability)

```
Tasks:
1. Long-term stability test (7 days)
2. Memory optimization for 100+ devices
3. Network stress testing
4. Error recovery comprehensive testing
```

**Use:**

- Sonnet for implementation
- Haiku for builds & tests
- Playwright for automated testing

### Week 3-4: Phase 3 (Performance)

```
Tasks:
1. Modbus polling optimization
2. MQTT batch publishing
3. Memory profiling & optimization
4. Stack size tuning
```

**Use:**

- Context7 for best practices research
- Opus for architecture decisions
- Arduino MCP for rapid iteration

### Week 5-6: Phase 4 (Security)

```
Tasks:
1. Authentication implementation
2. TLS/SSL for MQTT
3. Input validation
4. Security audit
```

**Use:**

- Context7 for security guidelines
- Opus for security architecture
- Playwright for penetration testing

### Week 7-8: Phase 5 (Testing)

```
Tasks:
1. Unit test suite
2. Integration tests
3. Stress testing
4. Field testing
```

**Use:**

- Haiku for test execution
- Playwright for automated testing
- Sonnet for test implementation

---

## 📚 Resources

### Official Documentation

- **MCP Protocol:** https://modelcontextprotocol.io/
- **Context7:** https://context7.com/
- **Playwright:** https://playwright.dev/
- **Arduino CLI:** https://arduino.github.io/arduino-cli/
- **ESP32 Arduino Core:** https://docs.espressif.com/projects/arduino-esp32/

### Project Documentation

- **CLAUDE.md** - AI assistant comprehensive guide
- **API.md** - BLE API reference
- **MODBUS_DATATYPES.md** - Modbus data types (40+ types)
- **VERSION_HISTORY.md** - Firmware changelog
- **LIBRARIES.md** - Arduino library dependencies

### Community & Support

- **GitHub Issues:** https://github.com/suriota/SRT-MGATE-1210-Firmware/issues
- **Email:** support@suriota.com
- **Website:** https://suriota.com

---

## 🎉 Conclusion

Dengan **3 MCP servers** (Arduino, Context7, Playwright), development workflow
Gateway Suriota menjadi:

✅ **Faster** - 4x productivity gain ✅ **Better** - Up-to-date documentation,
fewer errors ✅ **Smarter** - Automated testing, comprehensive validation ✅
**Production-Ready** - Following industry best practices

**Goal:** Firmware yang **handal, optimal, dan siap produksi** untuk
**komersialisasi** 🚀

**Timeline:** Production-ready dalam **2-3 bulan**

**Team:** SURIOTA R&D + Claude (AI Assistant)

---

**Made with ❤️ by SURIOTA R&D Team** **Powered by Claude Code + MCP Servers**
**Version 1.0.0 | Last Updated: November 24, 2025**
