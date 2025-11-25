# CLAUDE.md - AI Assistant Guide for SRT-MGATE-1210 Gateway

**Version:** 2.3.3 | **Last Updated:** November 22, 2025

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

## 🆕 v2.3.0 Features (Nov 21, 2025)

### BLE Enhancements
- **Backup/Restore:** Complete config export (200KB responses, PSRAM optimized)
- **Factory Reset:** One-command full device reset with auto-restart
- **Device Control:** Enable/disable devices via BLE with health metrics
- **Response Size:** Increased from 10KB → 200KB

**Documentation:** See `/Documentation/API_Reference/BLE_*.md` files

### Performance Evolution
- **Phase 1:** Two-tier logging (15% size reduction)
- **Phase 2:** Auto memory recovery with 3-tier thresholds
- **Phase 3:** RTC timestamps for accurate logging
- **Phase 4:** MTU negotiation timeout control (5s timeout, 2 retries, 100-byte fallback)
- **v2.1.1:** BLE transmission optimization (28x faster: 244-byte chunks, 10ms delay)
- **v2.2.0:** Config refactoring (HTTP interval in `http_config`)

---

## 📁 Key Directory Structure

```
Main/                    # Production firmware (PRIMARY)
  ├── Main.ino          # Entry point
  ├── DebugConfig.h     # ⚠️ MUST BE FIRST INCLUDE
  ├── ConfigManager.*   # JSON config with atomic writes
  ├── BLEManager.*      # BLE interface + fragmentation
  ├── CRUDHandler.*     # Command processor (priority queue)
  ├── NetworkManager.*  # Dual network failover
  ├── Modbus*Service.*  # RTU/TCP services
  ├── *Manager.*        # Mqtt, Http, Queue, etc.
  └── MemoryRecovery.*  # Auto memory monitoring

Documentation/          # Complete technical docs
  ├── API_Reference/API.md
  ├── Technical_Guides/MODBUS_DATATYPES.md (40+ types)
  └── Changelog/VERSION_HISTORY.md

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

**BLE Response Format (v2.1.1+):** ALWAYS return full objects:
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

## ⚠️ Critical Warnings

1. **Include Order:** `DebugConfig.h` MUST be first (defines LOG_* macros)
2. **PSRAM Fallback:** Always provide heap fallback for allocations
3. **Thread Safety:** Use mutexes for shared resources in FreeRTOS
4. **Atomic Writes:** Use `atomicWrite()` for critical configs (crash-safe)
5. **Full Objects:** Return complete objects in BLE CRUD responses (v2.1.1+)
6. **Notify Services:** Call `notifyConfigChange()` after config updates
7. **Hysteresis:** Respect network switching delays (prevent oscillation)
8. **Baudrate:** Restore original baudrate after Modbus device polling
9. **JSON Sizing:** Use `measureJson()` to allocate sufficient size
10. **Watchdog:** Feed watchdog with `vTaskDelay()` in long-running tasks

---

## 🆘 Quick Troubleshooting

| Issue | Solution |
|-------|----------|
| BLE connection hangs | Update to v2.2.0+ (MTU timeout handling) |
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
5. ✅ Return full objects in BLE CRUD (v2.1.1+)
6. ✅ Mutex protection for shared resources
7. ✅ Update VERSION_HISTORY.md for all changes
8. ✅ Respect network hysteresis delays
9. ✅ Monitor stack usage (`uxTaskGetStackHighWaterMark`)
10. ✅ Test in both dev and production modes

### Common File Locations
- Entry: `/Main/Main.ino`
- Configs: `/*.json` (devices, server_config, network_config, logging_config)
- Logging: `/Main/DebugConfig.h`, `/Main/LoggingConfig.h`
- Errors: `/Main/UnifiedErrorCodes.h`
- Memory: `/Main/MemoryRecovery.h`, `/Main/PSRAMValidator.h`
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

**Contact:** support@suriota.com | www.suriota.com | GitHub: suriota/SRT-MGATE-1210-Firmware

---

**End of CLAUDE.md**
