# CLAUDE.md - AI Assistant Guide for SRT-MGATE-1210 Gateway

**Version:** 1.0.0 | **Last Updated:** December 27, 2025

---

## 🎯 Project Overview

**SRT-MGATE-1210** is an Industrial IoT Gateway on ESP32-S3 for Modbus RTU/TCP data acquisition with MQTT/HTTP cloud connectivity.

**Platform:** ESP32-S3 (240MHz dual-core) | **Memory:** 512KB SRAM + 8MB PSRAM + 16MB Flash | **RTOS:** FreeRTOS (11+ tasks) | **Language:** C++ (Arduino)

**Protocols:** Modbus RTU (2x RS485), Modbus TCP, BLE 5.0, MQTT, HTTP/HTTPS

---

## 🏗️ Architecture

### Layered Design
```
Application Layer    → BLEManager, ButtonManager, LEDManager
Business Logic       → ModbusRtuService, ModbusTcpService, MqttManager, HttpManager
Infrastructure       → ConfigManager, NetworkManager, QueueManager, ErrorHandler
Platform Layer       → WiFiManager, EthernetManager, RTCManager, AtomicFileOps
```

**Design Patterns:** Singleton (managers), Factory (PSRAM allocation), Observer (config changes), Strategy (network failover)

### FreeRTOS Tasks
Core 1 priority tasks: MQTT, HTTP, RTU, TCP, BLE_CMD, BLE_STREAM, CRUD_Processor, Network_Monitor, LED, Button (priority 2)

---

## 🆕 Version 1.0.0 (Production Release - Dec 27, 2025)

### Core Features

#### Modbus Protocol Support
- **Modbus RTU** - Dual RS485 ports with dynamic baudrate switching (1200-115200)
- **Modbus TCP** - Connection pooling, auto-reconnect, multi-device support
- **40+ Data Types** - INT16, UINT32, FLOAT32, SWAP variants, BCD, ASCII
- **Calibration** - Per-register scale and offset support
- **Device Failure Tracking** - Exponential backoff, auto-disable, health metrics

#### Cloud Connectivity
- **MQTT** - TLS support, retain flag, unique client_id from MAC, auto-reconnect
- **HTTP/HTTPS** - REST API with certificate validation
- **Data Buffering** - Queue-based buffering during network outages

#### BLE Configuration Interface
- **CRUD API** - Full device/register/server configuration
- **OTA Updates** - Signed firmware from GitHub (public/private repos)
- **Backup/Restore** - Complete configuration export/import (up to 200KB)
- **Factory Reset** - One-command device reset
- **BLE Name Format** - `MGate-1210(P)XXXX` (4 hex chars from MAC)

#### Network Management
- **Dual Network** - WiFi + Ethernet with automatic failover
- **Network Status API** - Real-time connectivity monitoring

#### Memory Optimization
- **PSRAMString** - Unified PSRAM-based string allocation
- **Memory Recovery** - 4-tier automatic memory management
- **Shadow Copy Pattern** - Reduced file system I/O

#### Production Features
- **Two-Tier Logging** - Compile-time + runtime log level control
- **Unified Error Codes** - 7 domains, 60+ error codes
- **Atomic File Writes** - WAL pattern for crash-safe configuration

### Quality Metrics

| Category | Score |
|----------|-------|
| Logging System | 97% |
| Thread Safety | 96% |
| Memory Management | 95% |
| Error Handling | 93% |
| Overall | 91/100 |

**Development History:** See `Documentation/Archive/Development_Phase/` for complete changelog

---

## 📁 Key Directory Structure

```
Main/                    # Production firmware (PRIMARY)
  ├── Main.ino          # Entry point
  ├── ProductConfig.h   # Centralized product identity (version, model, variant)
  ├── DebugConfig.h     # MUST BE FIRST INCLUDE (defines LOG_* macros)
  ├── GatewayConfig.*   # Gateway identity (BLE name, serial number)
  ├── ConfigManager.*   # JSON config with atomic writes
  ├── BLEManager.*      # BLE interface + fragmentation
  ├── CRUDHandler.*     # Command processor (priority queue)
  ├── NetworkManager.*  # Dual network failover
  ├── Modbus*Service.*  # RTU/TCP services
  ├── *Manager.*        # Mqtt, Http, Queue, etc.
  └── MemoryRecovery.*  # Auto memory monitoring

Documentation/          # Complete technical docs
  ├── API_Reference/    # BLE CRUD API documentation
  ├── Technical_Guides/ # MODBUS_DATATYPES, PROTOCOL, LOGGING
  ├── Changelog/        # VERSION_HISTORY.md
  └── Archive/          # Development phase documentation

Testing/               # Test infrastructure
  ├── Device_Testing/  # Python BLE CRUD scripts
  └── Modbus_Simulators/  # RTU/TCP simulators
```

---

## 🔄 Development Workflow

### Production vs Development Mode
```cpp
#define PRODUCTION_MODE 0  // 0=Dev (verbose), 1=Prod (minimal logs, button BLE)
```

### Logging System (Two-Tier)
```cpp
// Compile-time
#define PRODUCTION_MODE 0  // Enables/disables macros

// Runtime
setLogLevel(LOG_INFO);  // NONE → ERROR → WARN → INFO → DEBUG → VERBOSE
```

**Module Macros:** `LOG_RTU_*`, `LOG_MQTT_*`, `LOG_BLE_*`, `LOG_CONFIG_*`, `LOG_NET_*`, `LOG_MEM_*`, etc.

### ⚠️ CRITICAL: Include Order
```cpp
#include "DebugConfig.h"     // ← MUST BE FIRST (defines LOG_* macros)
#include "MemoryRecovery.h"
#include "BLEManager.h"
// ... other includes
```

### Version Management
Update `VERSION_HISTORY.md` for ALL changes. Use semantic versioning: MAJOR.MINOR.PATCH

---

## 📝 Code Conventions

**Naming:** PascalCase (classes), camelCase (functions/vars), UPPER_CASE (constants)

**Singleton Pattern:**
```cpp
class MyManager {
private:
    static MyManager *instance;
    MyManager();  // Private
public:
    static MyManager *getInstance() {
        if (!instance) instance = new MyManager();
        return instance;
    }
    MyManager(const MyManager&) = delete;
    MyManager& operator=(const MyManager&) = delete;
};
```

**Error Handling:** Use `UnifiedErrorCode` enum (0-99: Network, 100-199: MQTT, 200-299: BLE, 300-399: Modbus, 400-499: Memory, 500-599: Config)

**BLE Response Format:** ALWAYS return full objects:
```json
{
  "status": "ok",
  "device_id": "D7A3F2",
  "data": { /* complete object */ }
}
```

---

## 💾 Memory Management

### Three-Tier Strategy
1. **PSRAM (8MB - PRIMARY):** Large objects, JSON docs, buffers, queues
2. **DRAM (512KB - FALLBACK):** Critical real-time ops, ISR, small objects
3. **AUTO RECOVERY:** WARNING → CRITICAL → EMERGENCY thresholds

**PSRAM Allocation with Fallback:**
```cpp
void* ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
if (ptr) {
    obj = new (ptr) MyClass();  // Placement new in PSRAM
} else {
    obj = new MyClass();  // Fallback to DRAM
}
```

**Setup Auto Recovery:**
```cpp
MemoryRecovery::setAutoRecovery(true);
MemoryRecovery::setCheckInterval(5000);  // 5s
MemoryRecovery::setMinFreeHeap(30000);   // 30KB threshold
```

**Thread Safety:** ALWAYS use mutexes for shared resources in FreeRTOS

---

## ⚙️ Configuration Management

### Files (LittleFS)
- `/devices.json` - Device configs
- `/server_config.json` - MQTT/HTTP settings
- `/logging_config.json` - Runtime log settings
- `/network_config.json` - WiFi/Ethernet config

### Atomic Write Pattern (WAL)
```cpp
bool ConfigManager::atomicWrite(const char *file, JsonDocument &doc) {
    String wal = String(file) + ".wal";
    // 1. Write to WAL
    File f = LittleFS.open(wal, "w");
    serializeJson(doc, f);
    f.close();
    // 2. Validate WAL
    // 3. Atomic rename
    return LittleFS.rename(wal.c_str(), file);
}
```

### Configuration Schema Examples
See `/Documentation/API_Reference/API.md` for complete schemas:
- **Device:** `device_name`, `protocol` (RTU/TCP), `slave_id`, `baud_rate`, `timeout`, `retry_count`, `refresh_rate_ms`, `enabled`, `registers[]`
- **Register:** `register_name`, `address`, `function_code` (1-4), `data_type` (see MODBUS_DATATYPES.md), `quantity`, `calibration` (optional)
- **Server:** `protocol` (mqtt/http), `mqtt{}`, `http{}`, `data_interval_ms`

**ALWAYS notify services after config changes:**
```cpp
if (saveConfig()) {
    modbusRtuService->notifyConfigChange();
    modbusTcpService->notifyConfigChange();
}
```

---

## 🔧 Common Tasks

### Adding Modbus Data Type
Edit `ModbusRtuService::parseDataType()` in `ModbusRtuService.cpp`. Update `/Documentation/Technical_Guides/MODBUS_DATATYPES.md`.

### Adding BLE CRUD Operation
Add operation in `CRUDHandler::processCommand()`. Update API.md. Add test script in `/Testing/Device_Testing/`.

### Adding Network Protocol
1. Create `*Manager.h/.cpp` following singleton pattern
2. Add log macros in `LoggingConfig.h`
3. Add error codes in `UnifiedErrorCodes.h`
4. Update `ServerConfig.h` for new protocol
5. Create FreeRTOS task in `Main.ino`
6. Update documentation

### Debugging Network Issues
```cpp
setLogLevel(LOG_DEBUG);
LOG_NET_INFO("Network: %s", currentNetwork == WIFI ? "WiFi" : "Ethernet");
NetworkHysteresis::canSwitchNetwork();  // Check failover status
```

---

## 🏗️ Build and Deployment

### Arduino IDE Settings
**Board:** ESP32-S3 Dev Module | **Flash:** 16MB | **PSRAM:** OPI PSRAM | **Partition:** Default 4MB SPIFFS | **Upload Speed:** 921600 | **CPU:** 240MHz

### Required Libraries
Install via Library Manager: ArduinoJson 7.4.2+, RTClib 2.1.4+, NTPClient 3.2.1+, Ethernet 2.0.2+, TBPubSubClient 2.12.1+, ModbusMaster 2.0.1+, OneButton 2.0+, ArduinoHttpClient 0.6.1+

See `/Documentation/Technical_Guides/LIBRARIES.md` for details.

### Compilation Modes
- **Development:** `PRODUCTION_MODE 0` - Full logging, BLE always on (~2.1MB)
- **Production:** `PRODUCTION_MODE 1` - Minimal logging, button BLE (~1.8MB, 15% smaller)

### Deployment Checklist
- [ ] Set `PRODUCTION_MODE = 1`
- [ ] Configure WiFi/MQTT in JSON files
- [ ] Test BLE button, network failover, Modbus communication
- [ ] 24-hour stability test
- [ ] Check memory usage (no leaks)
- [ ] Backup SD card files

---

## 📦 OTA Firmware Release

### Firmware Binary Naming Convention

**Format:** `{MODEL}_{VARIANT}_v{VERSION}.bin`

| Component | Description | Example |
|-----------|-------------|---------|
| MODEL | Product model code | `MGATE-1210` |
| VARIANT | Hardware variant (P=POE, omit for non-POE) | `P` or empty |
| VERSION | Semantic version | `1.0.0` |

**Examples:**
```
MGATE-1210_P_v1.0.0.bin      # POE variant, version 1.0.0
MGATE-1210_P_v1.2.5.bin      # POE variant, version 1.2.5
MGATE-1210_v1.0.0.bin        # Non-POE variant, version 1.0.0
MGATE-1210_v2.0.0.bin        # Non-POE variant, version 2.0.0
```

### OTA Release Process

```bash
# 1. Compile firmware in Arduino IDE (Sketch → Export Compiled Binary)

# 2. Sign firmware using Tools/sign_firmware.py
cd Tools
python sign_firmware.py ../Main/build/esp32.esp32.esp32s3/Main.ino.bin

# Script will prompt for version and variant, then output:
#   - MGATE-1210_P_v1.0.0.bin (properly named)
#   - firmware_manifest.json (with signature & checksum)

# 3. Copy to OTA repository
cp MGATE-1210_P_v1.0.0.bin /path/to/GatewaySuriotaOTA/releases/v1.0.0/
cp firmware_manifest.json /path/to/GatewaySuriotaOTA/

# 4. Update manifest URL to point to firmware
# URL format: https://api.github.com/repos/{owner}/{repo}/contents/releases/v{VERSION}/{FILENAME}?ref=main

# 5. Commit and push to OTA repo
git add releases/v1.0.0/ firmware_manifest.json
git commit -m "feat(v1.0.0): Release MGATE-1210 firmware"
git push
```

### OTA Repository Structure
```
GatewaySuriotaOTA/
├── firmware_manifest.json    # Root manifest (device checks this)
└── releases/
    ├── v1.0.0/
    │   └── MGATE-1210_P_v1.0.0.bin
    ├── v1.1.0/
    │   └── MGATE-1210_P_v1.1.0.bin
    └── ...
```

### Key Files
- **Signing Tool:** `Tools/sign_firmware.py` - ECDSA P-256 signature generator
- **Private Key:** `Tools/OTA_Keys/ota_private_key.pem` (NEVER commit to public repo!)
- **Public Key:** Embedded in firmware (`OTAManager.cpp`)

---

## ⚠️ Critical Warnings

1. **Include Order:** `DebugConfig.h` MUST be first (defines LOG_* macros)
2. **PSRAM Fallback:** Always provide heap fallback for allocations
3. **Thread Safety:** Use mutexes for shared resources in FreeRTOS
4. **Atomic Writes:** Use `atomicWrite()` for critical configs (crash-safe)
5. **Full Objects:** Return complete objects in BLE CRUD responses
6. **Notify Services:** Call `notifyConfigChange()` after config updates
7. **Hysteresis:** Respect network switching delays (prevent oscillation)
8. **Baudrate:** Restore original baudrate after Modbus device polling
9. **JSON Sizing:** Use `measureJson()` to allocate sufficient size
10. **Watchdog:** Feed watchdog with `vTaskDelay()` in long-running tasks

---

## 🆘 Quick Troubleshooting

| Issue | Solution |
|-------|----------|
| BLE connection hangs | Check MTU timeout handling in BLEManager |
| Memory exhaustion | Use `SpiRamJsonDocument`, check PSRAM allocation |
| Modbus no response | Check baudrate, slave ID, wiring, timeout settings |
| Network oscillation | Enable hysteresis (15s delay) in `network_config.json` |
| MQTT not publishing | Verify protocol="mqtt", broker address, network status |
| Log spam | Use `LogThrottle` class (e.g., 30s interval) |

**Detailed troubleshooting:** `/Documentation/Technical_Guides/TROUBLESHOOTING.md`

---

## 📚 Essential Resources

**Documentation:**
- `/Documentation/API_Reference/API.md` - Complete BLE CRUD API
- `/Documentation/Technical_Guides/MODBUS_DATATYPES.md` - 40+ data types
- `/Documentation/Technical_Guides/PROTOCOL.md` - BLE/Modbus protocols
- `/Documentation/Technical_Guides/LOGGING.md` - Debug log reference
- `/Documentation/Changelog/VERSION_HISTORY.md` - Version history

**Testing:**
- `/Testing/Device_Testing/` - Python BLE CRUD scripts
- `/Testing/Modbus_Simulators/` - RTU/TCP slave simulators

**External:**
- [ESP32-S3 Docs](https://docs.espressif.com/projects/arduino-esp32/)
- [Modbus Spec](https://modbus.org/docs/)
- [FreeRTOS Docs](https://www.freertos.org/Documentation/)

---

## 🎯 AI Assistant Quick Reference

### Essential Rules
1. ✅ `DebugConfig.h` first in any file using logging
2. ✅ PSRAM allocation with heap fallback for large objects
3. ✅ Atomic writes for config files (WAL pattern)
4. ✅ Notify services after config changes
5. ✅ Return full objects in BLE CRUD responses
6. ✅ Mutex protection for shared resources
7. ✅ Update VERSION_HISTORY.md for all changes
8. ✅ Respect network hysteresis delays
9. ✅ Monitor stack usage (`uxTaskGetStackHighWaterMark`)
10. ✅ Test in both dev and production modes

### Common File Locations
- Entry: `/Main/Main.ino`
- Product Config: `/Main/ProductConfig.h` (firmware version, model, variant, BLE format)
- Gateway Identity: `/Main/GatewayConfig.h` (BLE name generation, serial number)
- Configs: `/*.json` (devices, server_config, network_config, logging_config)
- Logging: `/Main/DebugConfig.h`, `/Main/LoggingConfig.h`
- Errors: `/Main/UnifiedErrorCodes.h`
- Memory: `/Main/MemoryRecovery.h`, `/Main/PSRAMValidator.h`
- OTA Signing: `/Tools/sign_firmware.py` (generates `MGATE-1210_{VARIANT}_v{VERSION}.bin`)
- OTA Keys: `/Tools/OTA_Keys/` (private/public key pair)
- Docs: `/Documentation/Changelog/VERSION_HISTORY.md`, `/Documentation/API_Reference/API.md`

### Code Templates

**FreeRTOS Task:**
```cpp
void myTask(void *param) {
    while (true) {
        // Task logic
        vTaskDelay(pdMS_TO_TICKS(1000));  // 1s delay
    }
}
// In setup()
xTaskCreatePinnedToCore(myTask, "Task", 8192, NULL, 1, NULL, 1);
```

**Memory Check:**
```cpp
size_t dram = heap_caps_get_free_size(MALLOC_CAP_8BIT);
size_t psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
LOG_MEM_INFO("Free - DRAM: %d, PSRAM: %d", dram, psram);
```

---

**Made with ❤️ by SURIOTA R&D Team** | *Industrial IoT Solutions*

**Contact:** support@suriota.com | www.suriota.com | GitHub: [GifariKemal/GatewaySuriotaPOC](https://github.com/GifariKemal/GatewaySuriotaPOC)

---

**End of CLAUDE.md**
