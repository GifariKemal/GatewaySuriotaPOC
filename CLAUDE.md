# CLAUDE.md - AI Assistant Guide for SRT-MGATE-1210 Gateway

**Version:** 2.2.0
**Last Updated:** November 20, 2025
**Repository:** GatewaySuriotaPOC (ESP32-S3 Industrial IoT Gateway)

---

## 📋 Table of Contents

1. [Project Overview](#-project-overview)
2. [Architecture Overview](#-architecture-overview)
3. [Directory Structure](#-directory-structure)
4. [Development Workflow](#-development-workflow)
5. [Code Conventions](#-code-conventions)
6. [Memory Management](#-memory-management)
7. [Configuration Management](#-configuration-management)
8. [Error Handling](#-error-handling)
9. [Testing Strategy](#-testing-strategy)
10. [Common Tasks](#-common-tasks)
11. [Build and Deployment](#-build-and-deployment)
12. [Important Warnings](#-important-warnings)

---

## 🎯 Project Overview

**SRT-MGATE-1210** is an Industrial IoT Gateway built on ESP32-S3 for Modbus RTU/TCP data acquisition with cloud connectivity via MQTT/HTTP. This is a production-grade firmware designed for reliability, performance, and maintainability.

### Key Characteristics

- **Platform:** ESP32-S3 (Xtensa LX7 dual-core @ 240MHz)
- **Memory:** 512KB SRAM + 8MB PSRAM + 16MB Flash
- **RTOS:** FreeRTOS with 11+ dedicated tasks
- **Language:** C++ (Arduino framework)
- **IDE:** Arduino IDE 2.0+
- **Current Version:** 2.2.0 (November 14, 2025)

### Primary Protocols

- **Modbus RTU:** 2x RS485 ports (1200-115200 baud)
- **Modbus TCP:** Multiple simultaneous connections
- **BLE 5.0:** Configuration interface with CRUD operations
- **MQTT:** Publish/subscribe with persistent queue
- **HTTP/HTTPS:** RESTful API client

---

## 🏗️ Architecture Overview

### Layered Architecture Pattern

```
┌─────────────────────────────────────────────────────┐
│    Application Layer (User Interface)              │
│        BLEManager, ButtonManager, LEDManager        │
├─────────────────────────────────────────────────────┤
│    Business Logic Layer (Services)                 │
│  ModbusRtuService, ModbusTcpService, CRUDHandler   │
│          MqttManager, HttpManager                   │
├─────────────────────────────────────────────────────┤
│    Infrastructure Layer (Core Managers)            │
│  ConfigManager, NetworkManager, QueueManager       │
│  ErrorHandler, MemoryRecovery, ServerConfig        │
├─────────────────────────────────────────────────────┤
│    Platform Layer (Hardware Abstraction)           │
│  WiFiManager, EthernetManager, RTCManager          │
│  AtomicFileOps, PSRAMValidator                     │
└─────────────────────────────────────────────────────┘
```

### Key Design Patterns

1. **Singleton Pattern:** All manager classes (NetworkMgr, ErrorHandler, QueueManager, etc.)
2. **Factory Pattern:** PSRAM allocation with heap fallback (`make_psram_unique`)
3. **Observer Pattern:** Configuration change notifications to services
4. **Strategy Pattern:** Network failover with configurable policies
5. **Command Pattern:** BLE CRUD operations with priority queues
6. **Template Method:** Atomic file operations with Write-Ahead Logging (WAL)

### FreeRTOS Task Architecture

| Task Name          | Core | Priority | Stack    | Responsibility               |
|--------------------|------|----------|----------|------------------------------|
| `MQTT_TASK`        | 1    | 1        | 8192     | MQTT publish loop            |
| `HTTP_TASK`        | 1    | 1        | 8192     | HTTP request processing      |
| `RTU_TASK`         | 1    | 1        | 16384    | Modbus RTU polling (2 ports) |
| `TCP_TASK`         | 1    | 1        | 16384    | Modbus TCP polling           |
| `BLE_CMD_TASK`     | 1    | 1        | 8192     | BLE command processor        |
| `BLE_STREAM_TASK`  | 1    | 1        | 4096     | Real-time data streaming     |
| `BLE_METRICS_TASK` | 1    | 0        | 4096     | Performance monitoring       |
| `CRUD_Processor`   | 1    | 1        | 8192     | Priority command execution   |
| `Network_Monitor`  | 1    | 1        | 4096     | Failover detection           |
| `LED_Blink_Task`   | 1    | 1        | 2048     | LED status indicators        |
| `Button_Task`      | 1    | 2        | 3072     | Button input (highest prio)  |

---

## 🚀 Advanced Features & Optimization

### Optimization Phases (Performance Evolution)

The firmware has undergone systematic optimization in phases:

**Phase 1: Log Level System**
- Two-tier logging (compile-time + runtime)
- Module-specific log macros
- Production mode optimization (15% firmware size reduction)
- Result: Reduced serial overhead, cleaner production builds

**Phase 2: Memory Recovery**
- Automatic DRAM/PSRAM monitoring
- Three-tier thresholds (WARNING → CRITICAL → EMERGENCY)
- Automatic cache clearing and connection management
- Result: Prevented memory exhaustion crashes

**Phase 3: RTC Timestamps**
- DS3231 RTC integration for accurate logging
- Timestamps format: `[YYYY-MM-DD HH:MM:SS]`
- Fallback to uptime if RTC unavailable
- Toggle-able via `setLogTimestamps(bool)`

**Phase 4: MTU Negotiation Timeout Control**
- 5-second timeout with 2 retries (total ~15s)
- Fallback to minimum MTU (100 bytes) on timeout
- State machine: IDLE → INITIATING → IN_PROGRESS → COMPLETED/TIMEOUT/FAILED
- Result: Eliminated mobile app connection hangs

**v2.1.1: BLE Transmission Optimization**
- Increased CHUNK_SIZE: 18 → 244 bytes (1356% increase)
- Reduced FRAGMENT_DELAY: 50ms → 10ms (80% reduction)
- Result: 28x faster transmission (58s → 2.1s for 21KB payload)

**v2.2.0: Configuration Refactoring**
- HTTP interval moved into `http_config` structure
- Eliminated redundant root-level `data_interval`
- Consistent API structure for all protocols

### Priority Queue & Batch Operations

**CRUDHandler** implements a sophisticated command execution system:

**Priority Levels:**
```cpp
enum class CommandPriority : uint8_t {
    PRIORITY_HIGH = 0,    // Emergency operations (config backup, emergency stop)
    PRIORITY_NORMAL = 1,  // Standard CRUD (default)
    PRIORITY_LOW = 2      // Background tasks (large reads, batch imports)
};
```

**Batch Execution Modes:**
```cpp
enum class BatchMode : uint8_t {
    SEQUENTIAL = 0,  // Execute commands one by one (safe, predictable)
    PARALLEL = 1,    // Execute simultaneously (faster, requires thread safety)
    ATOMIC = 2       // All-or-nothing (database-style transaction)
};
```

**Example - High Priority Command:**
```json
{
  "op": "update",
  "type": "server_config",
  "priority": "high",
  "config": {
    "protocol": "mqtt",
    "mqtt": { "broker_address": "new-broker.com" }
  }
}
```

**Example - Batch Operation (Sequential):**
```json
{
  "op": "batch",
  "batch_mode": "sequential",
  "commands": [
    {
      "op": "create",
      "type": "device",
      "config": { "device_name": "Sensor 1" }
    },
    {
      "op": "create",
      "type": "device",
      "config": { "device_name": "Sensor 2" }
    }
  ]
}
```

**Batch Statistics:**
```cpp
struct BatchStats {
    uint32_t totalBatchesProcessed;
    uint32_t totalCommandsProcessed;
    uint32_t highPriorityCount;
    uint32_t normalPriorityCount;
    uint32_t lowPriorityCount;
    uint32_t queuePeakDepth;
};
```

### Log Throttling (Preventing Log Spam)

**LogThrottle Class** prevents repetitive log messages from flooding the serial output:

```cpp
// In your service class
LogThrottle connectionThrottle(10000);  // 10 second interval

void checkConnection() {
    if (!isConnected()) {
        if (connectionThrottle.shouldLog()) {
            LOG_NET_ERROR("Connection lost to broker");
            // Logs: "(suppressed 47 similar messages)" if applicable
        }
    }
}
```

**Features:**
- Configurable interval (milliseconds)
- Automatic suppression counter
- Outputs suppressed count when resuming
- Thread-safe with internal state management

**Use Cases:**
- Network reconnection attempts
- Failed Modbus device polling
- MQTT publish failures in poor network conditions
- Memory warnings during sustained high load

### BLE Metrics & Performance Monitoring

**BLEManager** tracks comprehensive performance metrics:

**MTU Metrics:**
```cpp
struct MTUMetrics {
    uint16_t mtuSize;              // Current MTU (23-512 bytes)
    uint16_t maxMTUSize;           // Maximum negotiated MTU
    bool mtuNegotiated;            // Negotiation success status
    uint8_t timeoutCount;          // Number of timeouts
    unsigned long lastTimeoutTime; // Last timeout timestamp
};
```

**Connection Metrics:**
```cpp
struct ConnectionMetrics {
    unsigned long connectionStartTime;
    uint32_t fragmentsSent;
    uint32_t fragmentsReceived;
    uint32_t bytesTransmitted;
    uint32_t bytesReceived;
};
```

**Queue Metrics:**
```cpp
struct QueueMetrics {
    uint32_t currentDepth;
    uint32_t peakDepth;
    double utilizationPercent;
    uint32_t dropCount;
};
```

**Access Metrics:**
```json
{
  "op": "read",
  "type": "ble_metrics"
}
```

**Response:**
```json
{
  "status": "ok",
  "data": {
    "mtu": {
      "current": 512,
      "max": 512,
      "negotiated": true,
      "timeout_count": 0
    },
    "connection": {
      "uptime_sec": 3600,
      "fragments_sent": 1543,
      "bytes_tx": 156789
    },
    "queue": {
      "depth": 3,
      "peak": 15,
      "utilization": 30.0
    }
  }
}
```

### RTC Timestamps (Phase 3)

**Accurate Logging with DS3231 RTC:**

```cpp
// Enable timestamps (enabled by default)
setLogTimestamps(true);

// Example log output with RTC:
// [2025-11-20 14:35:22][INFO][RTU] Device D7A3F2 polling...

// If RTC not available, falls back to uptime:
// [12345][INFO][RTU] Device D7A3F2 polling...
```

**RTC Manager Features:**
- I2C communication with DS3231
- Automatic NTP synchronization (WiFi)
- Battery-backed time keeping
- Temperature compensation
- Alarm functionality (future use)

**Check RTC Status:**
```cpp
RTCManager *rtc = RTCManager::getInstance();
if (rtc->isRTCAvailable()) {
    DateTime now = rtc->now();
    LOG_RTC_INFO("RTC time: %04d-%02d-%02d %02d:%02d:%02d",
                 now.year(), now.month(), now.day(),
                 now.hour(), now.minute(), now.second());
}
```

### MTU Negotiation State Machine

**Handles MTU negotiation timeouts gracefully:**

```cpp
enum MTUNegotiationState {
    MTU_STATE_IDLE,        // No negotiation in progress
    MTU_STATE_INITIATING,  // Just started
    MTU_STATE_IN_PROGRESS, // Waiting for MTU exchange
    MTU_STATE_COMPLETED,   // Successfully negotiated
    MTU_STATE_TIMEOUT,     // Timed out
    MTU_STATE_FAILED       // Failed after retries
};
```

**Configuration:**
```cpp
struct MTUNegotiationControl {
    unsigned long negotiationTimeoutMs = 5000;  // 5-second timeout
    uint8_t maxRetries = 2;                     // 2 retries (total ~15s)
    uint16_t fallbackMTU = 100;                 // Safe minimum MTU
};
```

**Behavior:**
- **Timeout after 5s:** Retry negotiation
- **Max 2 retries:** Fall back to 100-byte MTU
- **Fallback mode:** Continue operation with minimal MTU
- **Result:** Mobile app never hangs, always connects

---

## 📁 Directory Structure

```
GatewaySuriotaPOC/
├── Main/                              # ⭐ Production firmware (PRIMARY)
│   ├── Main.ino                       # Entry point, setup(), loop()
│   ├── DebugConfig.h                  # ⚠️ MUST BE FIRST INCLUDE
│   ├── ConfigManager.h/.cpp           # JSON config storage (atomic writes)
│   ├── BLEManager.h/.cpp              # BLE interface with fragmentation
│   ├── CRUDHandler.h/.cpp             # Command processor (priority queue)
│   ├── NetworkManager.h/.cpp          # Dual network failover controller
│   ├── ModbusRtuService.h/.cpp        # Modbus RTU (2x RS485 ports)
│   ├── ModbusTcpService.h/.cpp        # Modbus TCP client
│   ├── MqttManager.h/.cpp             # MQTT with persistent queue
│   ├── HttpManager.h/.cpp             # HTTP/HTTPS REST client
│   ├── QueueManager.h/.cpp            # Circular buffer (PSRAM-optimized)
│   ├── ErrorHandler.h/.cpp            # Unified error codes
│   ├── MemoryRecovery.h/.cpp          # Auto memory monitoring
│   ├── AtomicFileOps.h/.cpp           # WAL for crash-safe writes
│   ├── PSRAMValidator.h               # Memory pre-validation
│   ├── ServerConfig.h/.cpp            # MQTT/HTTP endpoint config
│   ├── LoggingConfig.h/.cpp           # Runtime log control
│   ├── LEDManager.h/.cpp              # Status indicators
│   ├── ButtonManager.h/.cpp           # Mode control
│   ├── RTCManager.h/.cpp              # DS3231 RTC interface
│   ├── WiFiManager.h/.cpp             # WiFi connectivity
│   ├── EthernetManager.h/.cpp         # W5500 Ethernet
│   ├── NetworkHysteresis.h/.cpp       # Failover oscillation prevention
│   ├── UnifiedErrorCodes.h            # Error code definitions
│   └── (other support files)
│
├── Documentation/                     # Complete technical docs
│   ├── API_Reference/
│   │   └── API.md                     # BLE CRUD API reference
│   ├── Technical_Guides/
│   │   ├── MODBUS_DATATYPES.md        # 40+ data types reference
│   │   ├── PROTOCOL.md                # BLE/Modbus protocol details
│   │   ├── LOGGING.md                 # Debug log reference
│   │   ├── LIBRARIES.md               # Arduino library dependencies
│   │   └── HARDWARE.md                # GPIO pinout, schematics
│   ├── Changelog/
│   │   └── VERSION_HISTORY.md         # Version changelog (v2.2.0)
│   ├── Optimizing/
│   │   └── test_log_system/           # Optimization test code
│   └── Archive/                       # Historical documents
│
├── Testing/                           # Test infrastructure
│   ├── BLE_Testing/                   # BLE communication tests
│   ├── Device_Testing/
│   │   ├── RTU_Create/                # Python scripts for RTU devices
│   │   └── TCP_Create/                # Python scripts for TCP devices
│   ├── Modbus_Simulators/
│   │   ├── RTU_Slave/                 # Software RTU slave simulators
│   │   └── TCP_Slave/                 # Software TCP slave simulators
│   ├── Server_Config/                 # MQTT/HTTP config tests
│   └── Old_Testing/                   # Archived tests
│
├── Sample Code Test HW Related/       # Hardware integration tests
│   ├── All_function/                  # Complete integration test
│   ├── LED_blink/                     # LED peripheral test
│   ├── Button/                        # Button input test
│   ├── RTC_ds3231/                    # RTC I2C test
│   ├── SD_Test/                       # SD card test
│   ├── ETH_WebServer/                 # Ethernet connectivity test
│   ├── RS485_modbus/                  # RS485 communication test
│   ├── Firebase_DB/                   # Firebase integration test
│   └── (other HW tests)
│
├── Mockup UI/                         # UI design mockups
├── README.md                          # Project overview & quick start
└── LICENSE                            # MIT License
```

### Key Directory Guidelines

**When working on production code:**
- **ALWAYS** modify files in `/Main/` directory
- **NEVER** edit test code unless explicitly testing

**When adding features:**
- Update `/Documentation/Changelog/VERSION_HISTORY.md`
- Update `/Documentation/API_Reference/API.md` if API changes
- Create tests in `/Testing/` if appropriate

**When debugging:**
- Check `/Documentation/Technical_Guides/LOGGING.md` for log reference
- Check `/Documentation/Technical_Guides/TROUBLESHOOTING.md` for common issues

---

## 🔄 Development Workflow

### 1. Production vs Development Mode

The firmware has two modes controlled by a single `#define`:

```cpp
// In Main/Main.ino
#define PRODUCTION_MODE 0  // 0 = Development, 1 = Production
```

**Development Mode (`PRODUCTION_MODE = 0`):**
- BLE always ON (auto-start)
- All logging enabled by default
- Serial Monitor verbose output
- Debug features accessible

**Production Mode (`PRODUCTION_MODE = 1`):**
- BLE disabled by default (button-controlled)
- Minimal logging (ERROR + WARN only)
- No debug output
- Optimized for deployment

### 2. Logging System (Two-Tier)

**Compile-Time Control:**
```cpp
#define PRODUCTION_MODE 0  // Enables/disables logging macros
```

**Runtime Control:**
```cpp
// In setup()
setLogLevel(LOG_INFO);  // LOG_NONE → ERROR → WARN → INFO → DEBUG → VERBOSE
```

**Module-Specific Macros:**
```cpp
LOG_RTU_INFO("Device %s polling...", deviceId.c_str());
LOG_MQTT_ERROR("Publish failed: %d", errorCode);
LOG_BLE_DEBUG("MTU negotiated: %d bytes", mtu);
LOG_CONFIG_WARN("File not found: %s", filename);
```

**Available Log Modules:**
- `LOG_RTU_*` - Modbus RTU service
- `LOG_TCP_*` - Modbus TCP service
- `LOG_MQTT_*` - MQTT manager
- `LOG_HTTP_*` - HTTP manager
- `LOG_BLE_*` - BLE manager
- `LOG_CONFIG_*` - Configuration manager
- `LOG_NET_*` - Network manager
- `LOG_MEM_*` - Memory management
- `LOG_QUEUE_*` - Queue manager
- `LOG_ERROR_*` - Error handler

### 3. Include Order Convention

**CRITICAL: DebugConfig.h MUST BE FIRST**

```cpp
// ✅ CORRECT ORDER
#include "DebugConfig.h"        // ← MUST BE FIRST (defines logging macros)
#include "MemoryRecovery.h"     // Phase 2 optimization
#include "BLEManager.h"
#include "ConfigManager.h"
// ... other includes

// ❌ WRONG ORDER - Will cause compilation errors
#include "BLEManager.h"         // ← WRONG! DebugConfig must be first
#include "DebugConfig.h"
```

**Reason:** `DebugConfig.h` defines all `LOG_*` macros used throughout the codebase. Including it after other headers will cause undefined macro errors.

### 4. Version Management

**Update VERSION_HISTORY.md for ALL changes:**

```markdown
### Version X.Y.Z (YYYY-MM-DD)

**Feature Type** (Performance / Feature / Bugfix / Documentation)

**Developer:** [Your Name] | **Release Date:** [Date] - WIB (GMT+7)

#### Changes
- ✅ Feature/fix description
- ✅ Impact and benefits
```

**Semantic Versioning:**
- **MAJOR (X):** Breaking API changes
- **MINOR (Y):** New features (backward compatible)
- **PATCH (Z):** Bug fixes (backward compatible)

---

## 📝 Code Conventions

### 1. Naming Conventions

**Classes:**
```cpp
class ConfigManager {};      // PascalCase
class ModbusRtuService {};   // PascalCase with acronyms
```

**Functions/Methods:**
```cpp
void loadConfig();           // camelCase
bool validateDevice();       // camelCase
```

**Variables:**
```cpp
int deviceCount;             // camelCase
const int MAX_DEVICES = 50;  // UPPER_CASE for constants
```

**Files:**
```cpp
ConfigManager.h/.cpp         // Match class name (PascalCase)
ModbusRtuService.h/.cpp      // Match class name
```

### 2. Header Guard Convention

```cpp
#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

// ... declarations ...

#endif // CONFIG_MANAGER_H
```

### 3. Singleton Pattern Convention

```cpp
class NetworkMgr {
private:
    static NetworkMgr *instance;
    NetworkMgr();  // Private constructor

public:
    static NetworkMgr *getInstance() {
        if (instance == nullptr) {
            instance = new NetworkMgr();
        }
        return instance;
    }

    // Delete copy constructor and assignment
    NetworkMgr(const NetworkMgr&) = delete;
    NetworkMgr& operator=(const NetworkMgr&) = delete;
};

// In .cpp file
NetworkMgr *NetworkMgr::instance = nullptr;
```

### 4. Error Handling Convention

**Use Unified Error Codes:**
```cpp
// UnifiedErrorCodes.h defines error ranges
enum UnifiedErrorCode {
    // Network errors (0-99)
    ERR_NET_WIFI_DISCONNECTED = 1,
    ERR_NET_ETH_CABLE_UNPLUGGED = 2,

    // MQTT errors (100-199)
    ERR_MQTT_CONNECTION_FAILED = 101,
    ERR_MQTT_PUBLISH_FAILED = 102,

    // BLE errors (200-299)
    ERR_BLE_INIT_FAILED = 201,
    ERR_BLE_MTU_NEGOTIATION_TIMEOUT = 202,

    // Modbus errors (300-399)
    ERR_MODBUS_DEVICE_TIMEOUT = 301,
    ERR_MODBUS_INVALID_RESPONSE = 302,

    // Memory errors (400-499)
    ERR_MEMORY_ALLOCATION_FAILED = 401,
    ERR_MEMORY_PSRAM_INIT_FAILED = 402,

    // Config errors (500-599)
    ERR_CONFIG_FILE_NOT_FOUND = 501,
    ERR_CONFIG_JSON_PARSE_FAILED = 502,
};
```

**Report Errors:**
```cpp
ErrorHandler::getInstance()->reportError(
    ERR_MODBUS_DEVICE_TIMEOUT,
    deviceId,
    timeoutMs
);
```

### 5. Configuration Change Convention

**Always notify services after config changes:**
```cpp
bool ConfigManager::updateDevice(const JsonObject &device) {
    // 1. Update config file
    if (!saveDevicesConfig(devices)) {
        return false;
    }

    // 2. Invalidate cache
    cacheDevices.clear();

    // 3. Notify affected services
    if (modbusRtuService) {
        modbusRtuService->notifyConfigChange();
    }
    if (modbusTcpService) {
        modbusTcpService->notifyConfigChange();
    }

    return true;
}
```

### 6. BLE Response Format (v2.1.1+)

**ALL CRUD operations return full objects:**

```cpp
// CREATE response
{
    "status": "ok",
    "device_id": "D7A3F2",
    "data": {                    // ← Full created object
        "device_name": "Temp Sensor",
        "protocol": "RTU",
        // ... complete device config
    }
}

// UPDATE response
{
    "status": "ok",
    "device_id": "D7A3F2",
    "data": {                    // ← Full updated object
        "device_name": "Updated Name",
        // ... complete updated config
    }
}

// DELETE response
{
    "status": "ok",
    "device_id": "D7A3F2",
    "deleted_data": {            // ← Full deleted object
        "device_name": "Temp Sensor",
        // ... complete deleted config
    }
}
```

**Mobile App Benefit:** Immediate UI updates without additional READ requests.

---

## 💾 Memory Management

### Three-Tier Memory Strategy

**1. PSRAM (External 8MB) - PRIMARY:**

```cpp
// Allocate large objects in PSRAM with fallback
void* ptr = heap_caps_malloc(sizeof(ConfigManager),
                              MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
if (ptr) {
    configManager = new (ptr) ConfigManager();  // Placement new
    LOG_MEM_INFO("ConfigManager allocated in PSRAM");
} else {
    configManager = new ConfigManager();  // Fallback to heap
    LOG_MEM_WARN("ConfigManager allocated in DRAM (PSRAM failed)");
}
```

**2. DRAM (Internal 512KB) - FALLBACK:**
- Critical real-time operations
- ISR-safe allocations
- Small objects (<1KB)

**3. AUTOMATIC RECOVERY:**

```cpp
// In setup()
MemoryRecovery::setAutoRecovery(true);
MemoryRecovery::setCheckInterval(5000);  // Check every 5s
MemoryRecovery::setMinFreeHeap(30000);   // 30KB warning threshold
```

**Recovery Actions:**
- **WARNING:** Log memory status
- **CRITICAL:** Clear caches, close connections
- **EMERGENCY:** System restart

### Memory Allocation Guidelines

**ALWAYS allocate in PSRAM:**
- ConfigManager (>100KB with devices)
- CRUDHandler (large JSON buffers)
- BLEManager (MTU buffers)
- JSON documents (ArduinoJson)
- Queue buffers
- Task stacks (non-critical tasks)

**NEVER allocate in PSRAM:**
- ISR handlers
- Critical timing operations
- DMA buffers

### Smart Pointer Pattern

```cpp
// Custom PSRAM deleter
template <typename T>
struct PsramTypeDeleter {
    void operator()(T *ptr) {
        if (ptr) {
            ptr->~T();
            heap_caps_free(ptr);
        }
    }
};

template <typename T>
using PsramUniquePtr = std::unique_ptr<T, PsramTypeDeleter<T>>;

// Factory function
template <typename T, typename... Args>
PsramUniquePtr<T> make_psram_unique(Args &&...args) {
    void *mem = heap_caps_malloc(sizeof(T),
                                  MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (mem) {
        return PsramUniquePtr<T>(new (mem) T(std::forward<Args>(args)...));
    }
    return PsramUniquePtr<T>(nullptr);
}
```

### Thread Safety

**ALWAYS protect shared resources:**

```cpp
class ConfigManager {
private:
    SemaphoreHandle_t cacheMutex;
    SemaphoreHandle_t fileMutex;

public:
    bool readConfig() {
        xSemaphoreTake(cacheMutex, portMAX_DELAY);
        // ... critical section ...
        xSemaphoreGive(cacheMutex);
        return success;
    }
};
```

---

## ⚙️ Configuration Management

### File System: LittleFS with Atomic Writes

**Configuration Files:**
```
/devices.json           # Device configurations
/registers.json         # Register mappings (deprecated, now in devices.json)
/server_config.json     # MQTT/HTTP settings
/logging_config.json    # Runtime log settings
/network_config.json    # WiFi/Ethernet config
```

### Atomic Write Pattern (WAL - Write-Ahead Logging)

```cpp
bool ConfigManager::atomicWrite(const char *filename, JsonDocument &doc) {
    // 1. Write to temporary WAL file
    String walPath = String(filename) + ".wal";
    File walFile = LittleFS.open(walPath.c_str(), "w");
    if (!walFile) {
        LOG_CONFIG_ERROR("Failed to open WAL: %s", walPath.c_str());
        return false;
    }
    serializeJson(doc, walFile);
    walFile.close();

    // 2. Validate WAL file
    File validateFile = LittleFS.open(walPath.c_str(), "r");
    JsonDocument validateDoc;
    if (deserializeJson(validateDoc, validateFile) != DeserializationError::Ok) {
        LOG_CONFIG_ERROR("WAL validation failed");
        validateFile.close();
        return false;
    }
    validateFile.close();

    // 3. Atomic rename (guaranteed atomic on LittleFS)
    if (LittleFS.rename(walPath.c_str(), filename)) {
        LOG_CONFIG_INFO("Atomic write successful: %s", filename);
        return true;
    }

    LOG_CONFIG_ERROR("Atomic rename failed");
    return false;
}
```

### Caching Strategy

```cpp
class ConfigManager {
private:
    std::map<String, JsonDocument> cacheDevices;
    unsigned long cacheTimestamp;
    const unsigned long CACHE_TTL_MS = 600000;  // 10 minutes

public:
    JsonDocument loadDevices() {
        // Check cache first
        if (cacheDevices.count("devices") > 0) {
            if (millis() - cacheTimestamp < CACHE_TTL_MS) {
                LOG_CONFIG_DEBUG("Returning cached devices");
                return cacheDevices["devices"];
            }
        }

        // Cache miss or expired - load from file
        JsonDocument doc = loadFromFile("/devices.json");
        cacheDevices["devices"] = doc;
        cacheTimestamp = millis();
        return doc;
    }
};
```

### Configuration Schema

**Device Configuration:**
```json
{
  "devices": {
    "D7A3F2": {
      "device_name": "Temperature Sensor",
      "protocol": "RTU",           // RTU or TCP
      "slave_id": 1,               // 1-247 for RTU
      "serial_port": 1,            // 1 or 2 (RTU only)
      "baud_rate": 9600,           // 1200-115200 (RTU only)
      "timeout": 3000,             // Milliseconds
      "retry_count": 3,            // Max retries before disabling
      "refresh_rate_ms": 5000,     // Device-level refresh interval
      "enabled": true,             // Enable/disable polling
      "registers": [
        {
          "register_name": "Temperature",
          "address": 0,            // Register address
          "function_code": 3,      // 1=Coil, 2=DI, 3=HR, 4=IR
          "data_type": "FLOAT32_ABCD",  // See MODBUS_DATATYPES.md
          "quantity": 2,           // Register count (FLOAT32=2)
          "refresh_rate_ms": 1000, // Register-level override (optional)
          "calibration": {         // Optional calibration
            "scale": 1.0,
            "offset": 0.0
          }
        }
      ]
    }
  }
}
```

**Server Configuration:**
```json
{
  "protocol": "mqtt",              // "mqtt" or "http"
  "mqtt": {
    "broker_address": "broker.hivemq.com",
    "broker_port": 1883,
    "topic_publish": "suriota/gateway/data",
    "client_id": "SURIOTA_GW_001",
    "username": "",                // Optional
    "password": "",                // Optional
    "qos": 1                       // 0, 1, or 2
  },
  "http": {
    "server_url": "https://api.example.com/data",
    "method": "POST",              // POST, PUT, PATCH
    "interval_ms": 5000,           // HTTP polling interval
    "headers": {                   // Optional custom headers
      "Authorization": "Bearer token"
    }
  },
  "data_interval_ms": 5000         // Global publish interval
}
```

**Network Configuration:**
```json
{
  "wifi": {
    "ssid": "MyNetwork",
    "password": "password123",
    "priority": 2                  // Lower = higher priority (WiFi=fallback)
  },
  "ethernet": {
    "enabled": true,
    "priority": 1                  // Lower = higher priority (Ethernet=primary)
  },
  "failover": {
    "enabled": true,
    "check_interval_ms": 5000,     // Check network health every 5s
    "hysteresis_ms": 10000         // Prevent rapid switching
  }
}
```

---

## 🚨 Error Handling

### Unified Error Code System

**Error Code Ranges:**
```cpp
// UnifiedErrorCodes.h
enum UnifiedErrorCode {
    // Network errors (0-99)
    ERR_NET_WIFI_DISCONNECTED = 1,
    ERR_NET_ETH_CABLE_UNPLUGGED = 2,
    ERR_NET_DHCP_FAILED = 3,
    ERR_NET_FAILOVER_TRIGGERED = 10,

    // MQTT errors (100-199)
    ERR_MQTT_CONNECTION_FAILED = 101,
    ERR_MQTT_PUBLISH_FAILED = 102,
    ERR_MQTT_BROKER_UNREACHABLE = 103,

    // BLE errors (200-299)
    ERR_BLE_INIT_FAILED = 201,
    ERR_BLE_MTU_NEGOTIATION_TIMEOUT = 202,
    ERR_BLE_ADVERTISING_START_FAILED = 203,

    // Modbus errors (300-399)
    ERR_MODBUS_DEVICE_TIMEOUT = 301,
    ERR_MODBUS_INVALID_RESPONSE = 302,
    ERR_MODBUS_CRC_ERROR = 303,
    ERR_MODBUS_EXCEPTION_CODE = 304,

    // Memory errors (400-499)
    ERR_MEMORY_ALLOCATION_FAILED = 401,
    ERR_MEMORY_PSRAM_INIT_FAILED = 402,
    ERR_MEMORY_HEAP_EXHAUSTED = 403,

    // Config errors (500-599)
    ERR_CONFIG_FILE_NOT_FOUND = 501,
    ERR_CONFIG_JSON_PARSE_FAILED = 502,
    ERR_CONFIG_WRITE_FAILED = 503,
    ERR_CONFIG_INVALID_FORMAT = 504,
};
```

### Error Reporting Pattern

```cpp
// ErrorHandler.h
class ErrorHandler {
public:
    static ErrorHandler* getInstance();

    void reportError(UnifiedErrorCode code,
                     const String &context = "",
                     int detail = 0);

    void reportWarning(UnifiedErrorCode code,
                       const String &context = "");

    String getErrorMessage(UnifiedErrorCode code);

    // Statistics
    int getErrorCount(UnifiedErrorCode code);
    void clearErrorCount(UnifiedErrorCode code);
};

// Usage example
void ModbusRtuService::pollDevice(const Device &device) {
    uint8_t result = modbus.readHoldingRegisters(address, quantity);

    if (result != modbus.ku8MBSuccess) {
        ErrorHandler::getInstance()->reportError(
            ERR_MODBUS_DEVICE_TIMEOUT,
            device.device_id,
            device.timeout_ms
        );

        LOG_RTU_ERROR("Device %s timeout (Slave:%d Port:%d)",
                      device.device_id.c_str(),
                      device.slave_id,
                      device.serial_port);
        return;
    }
}
```

### Exponential Backoff Pattern

```cpp
class ModbusRtuService {
private:
    struct DeviceRetry {
        int retryCount;
        unsigned long nextRetryTime;
        unsigned long backoffDelay;
    };

    std::map<String, DeviceRetry> deviceRetries;

    void handleDeviceFailure(const String &deviceId) {
        DeviceRetry &retry = deviceRetries[deviceId];
        retry.retryCount++;

        if (retry.retryCount >= device.max_retries) {
            // Disable device after max retries
            device.enabled = false;
            LOG_RTU_WARN("Device %s disabled after %d failures",
                         deviceId.c_str(), retry.retryCount);
            return;
        }

        // Exponential backoff: 2s → 4s → 8s → 16s → 32s
        retry.backoffDelay = min(2000 * pow(2, retry.retryCount - 1), 32000);
        retry.nextRetryTime = millis() + retry.backoffDelay;

        LOG_RTU_INFO("Device %s retry %d/%d in %lums",
                     deviceId.c_str(),
                     retry.retryCount,
                     device.max_retries,
                     retry.backoffDelay);
    }
};
```

---

## 🧪 Testing Strategy

### Multi-Level Testing Approach

**1. Hardware Integration Tests** (`/Sample Code Test HW Related/`)

Purpose: Validate individual hardware peripherals

```
All_function/         # Complete integration test (all peripherals)
LED_blink/            # GPIO output test
Button/               # GPIO input test (interrupts)
RTC_ds3231/           # I2C communication test
SD_Test/              # SPI SD card test
ETH_WebServer/        # W5500 Ethernet test
RS485_modbus/         # RS485 transceiver test
Firebase_DB/          # WiFi + cloud integration test
```

**Usage:**
```bash
# 1. Open test sketch in Arduino IDE
# 2. Upload to ESP32-S3
# 3. Open Serial Monitor (115200 baud)
# 4. Observe test results
```

**2. Device Testing** (`/Testing/Device_Testing/`)

Purpose: BLE CRUD operation validation with Python scripts

**RTU Device Creation:**
```bash
cd Testing/Device_Testing/RTU_Create
pip install -r requirements.txt
python create_rtu_device.py
```

**TCP Device Creation:**
```bash
cd Testing/Device_Testing/TCP_Create
pip install -r requirements.txt
python create_tcp_device.py
```

**3. Modbus Simulators** (`/Testing/Modbus_Simulators/`)

Purpose: Test Modbus communication without physical devices

**RTU Slave Simulator:**
```bash
cd Testing/Modbus_Simulators/RTU_Slave
pip install -r requirements.txt
python rtu_slave_simulator.py --port COM3 --slave-id 1 --baudrate 9600
```

**TCP Slave Simulator:**
```bash
cd Testing/Modbus_Simulators/TCP_Slave
pip install -r requirements.txt
python tcp_slave_simulator.py --port 502
```

**4. BLE Testing** (`/Testing/BLE_Testing/`)

Purpose: Validate BLE communication, MTU negotiation, fragmentation

**5. Server Configuration** (`/Testing/Server_Config/`)

Purpose: Test MQTT/HTTP endpoint configurations

### Testing Conventions

**All test directories include:**
- `00_START_HERE.txt` or `README.md` - Quick start guide
- `requirements.txt` - Python dependencies (pinned versions)
- `DOCUMENTATION.md` - Detailed test explanation
- Test scripts with clear comments

**When adding new tests:**
1. Create appropriate subdirectory in `/Testing/`
2. Include `README.md` with purpose and usage
3. Add `requirements.txt` for Python tests
4. Document expected results
5. Update main README.md if major test

---

## 🔧 Common Tasks

### Task 1: Adding a New Modbus Data Type

**File to modify:** `Main/ModbusRtuService.cpp` or `Main/ModbusTcpService.cpp`

```cpp
float ModbusRtuService::parseDataType(const String &dataType,
                                      uint16_t *buffer,
                                      int quantity) {
    // Existing types: INT16, UINT16, INT32_ABCD, FLOAT32_ABCD, etc.

    // Add new type: UINT64_ABCD_CDGH (8 registers)
    if (dataType == "UINT64_ABCD_CDGH") {
        if (quantity < 4) {
            LOG_RTU_ERROR("UINT64 requires 4 registers");
            return 0.0;
        }

        uint64_t value = 0;
        value |= ((uint64_t)buffer[0] << 48);  // A
        value |= ((uint64_t)buffer[1] << 32);  // B
        value |= ((uint64_t)buffer[2] << 16);  // C
        value |= ((uint64_t)buffer[3] << 0);   // D

        return (float)value;
    }

    // ... handle other types
}
```

**Don't forget:**
1. Update `Documentation/Technical_Guides/MODBUS_DATATYPES.md`
2. Add test case in simulator
3. Update `Documentation/Changelog/VERSION_HISTORY.md`

### Task 2: Adding a New BLE CRUD Operation

**File to modify:** `Main/CRUDHandler.cpp`

```cpp
void CRUDHandler::processCommand(JsonDocument &doc) {
    String op = doc["op"].as<String>();
    String type = doc["type"].as<String>();

    // Existing: create, read, update, delete

    // Add new operation: "clone"
    if (op == "clone") {
        if (type == "device") {
            handleCloneDevice(doc);
        }
        return;
    }
}

void CRUDHandler::handleCloneDevice(JsonDocument &doc) {
    String sourceId = doc["source_id"].as<String>();
    String newId = generateDeviceId();

    // 1. Read source device
    JsonDocument sourceDoc = configManager->loadDevice(sourceId);
    if (sourceDoc.isNull()) {
        sendErrorResponse("Source device not found");
        return;
    }

    // 2. Clone with new ID
    JsonDocument clonedDoc;
    clonedDoc["device_id"] = newId;
    clonedDoc["device_name"] = sourceDoc["device_name"].as<String>() + " (Clone)";
    // ... copy other fields

    // 3. Save cloned device
    if (configManager->createDevice(clonedDoc)) {
        sendSuccessResponse(clonedDoc);
    } else {
        sendErrorResponse("Clone failed");
    }
}
```

**Don't forget:**
1. Update `Documentation/API_Reference/API.md` with new operation
2. Update mobile app to support new operation
3. Add test script in `/Testing/Device_Testing/`
4. Update `Documentation/Changelog/VERSION_HISTORY.md`

### Task 3: Adding a New Network Protocol (e.g., CoAP)

**Files to create:**
- `Main/CoapManager.h`
- `Main/CoapManager.cpp`

**Pattern to follow:**

```cpp
// CoapManager.h
#ifndef COAP_MANAGER_H
#define COAP_MANAGER_H

#include <Arduino.h>
#include <coap-simple.h>  // Example CoAP library

class CoapManager {
private:
    static CoapManager *instance;
    Coap coap;
    bool initialized;

    CoapManager();

public:
    static CoapManager *getInstance();

    bool begin(const char *serverUrl, int port);
    bool publish(const String &data);
    void loop();  // Must be called in main loop

    // Singleton pattern
    CoapManager(const CoapManager&) = delete;
    CoapManager& operator=(const CoapManager&) = delete;
};

#endif
```

```cpp
// CoapManager.cpp
#include "CoapManager.h"
#include "LoggingConfig.h"

CoapManager *CoapManager::instance = nullptr;

CoapManager::CoapManager() : initialized(false) {}

CoapManager *CoapManager::getInstance() {
    if (instance == nullptr) {
        instance = new CoapManager();
    }
    return instance;
}

bool CoapManager::begin(const char *serverUrl, int port) {
    if (initialized) {
        LOG_COAP_WARN("Already initialized");
        return true;
    }

    // Initialize CoAP client
    coap.server(callback_response);
    coap.start();

    initialized = true;
    LOG_COAP_INFO("CoAP client started: %s:%d", serverUrl, port);
    return true;
}

bool CoapManager::publish(const String &data) {
    if (!initialized) {
        LOG_COAP_ERROR("Not initialized");
        return false;
    }

    // Publish via CoAP
    int msgId = coap.put(serverUrl, data.c_str(), data.length());

    if (msgId != 0) {
        LOG_COAP_INFO("Published data (msgId: %d)", msgId);
        return true;
    }

    LOG_COAP_ERROR("Publish failed");
    return false;
}

void CoapManager::loop() {
    if (initialized) {
        coap.loop();
    }
}
```

**Don't forget:**
1. Add log macros in `Main/LoggingConfig.h`:
   ```cpp
   #define LOG_COAP_INFO(...)  LOGF(LOG_INFO, "[CoAP] ", __VA_ARGS__)
   #define LOG_COAP_ERROR(...) LOGF(LOG_ERROR, "[CoAP] ", __VA_ARGS__)
   ```
2. Add error codes in `Main/UnifiedErrorCodes.h`:
   ```cpp
   enum UnifiedErrorCode {
       // CoAP errors (600-699)
       ERR_COAP_CONNECTION_FAILED = 601,
       ERR_COAP_PUBLISH_FAILED = 602,
   };
   ```
3. Update `Main/ServerConfig.h` to support CoAP configuration
4. Create FreeRTOS task in `Main/Main.ino`:
   ```cpp
   void coapTask(void *parameter) {
       CoapManager *coap = CoapManager::getInstance();
       while (true) {
           coap->loop();
           vTaskDelay(pdMS_TO_TICKS(100));
       }
   }
   ```
5. Update documentation:
   - `Documentation/API_Reference/API.md`
   - `Documentation/Technical_Guides/PROTOCOL.md`
   - `Documentation/Changelog/VERSION_HISTORY.md`

### Task 4: Optimizing Memory Usage

**Use MemoryRecovery monitoring:**

```cpp
// Enable memory monitoring
MemoryRecovery::setAutoRecovery(true);
MemoryRecovery::setCheckInterval(5000);  // Check every 5s
MemoryRecovery::setMinFreeHeap(30000);   // 30KB warning

// Manual memory check
void checkMemoryUsage() {
    LOG_MEM_INFO("Free DRAM: %d bytes", heap_caps_get_free_size(MALLOC_CAP_8BIT));
    LOG_MEM_INFO("Free PSRAM: %d bytes", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    LOG_MEM_INFO("Largest DRAM block: %d bytes",
                 heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}
```

**Common optimization strategies:**

1. **Move large objects to PSRAM:**
   ```cpp
   // Before: allocated in DRAM
   JsonDocument doc(8192);

   // After: allocated in PSRAM
   SpiRamJsonDocument doc(8192);
   ```

2. **Reduce JSON document sizes:**
   ```cpp
   // Use minimal JSON when possible
   JsonDocument doc(512);  // Instead of 2048
   ```

3. **Clear caches periodically:**
   ```cpp
   void ConfigManager::clearCache() {
       xSemaphoreTake(cacheMutex, portMAX_DELAY);
       cacheDevices.clear();
       cacheTimestamp = 0;
       xSemaphoreGive(cacheMutex);
       LOG_CONFIG_INFO("Cache cleared");
   }
   ```

4. **Reduce task stack sizes:**
   ```cpp
   // Profile actual stack usage first
   UBaseType_t stackHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
   LOG_MEM_INFO("Task stack high water mark: %d", stackHighWaterMark);

   // Reduce if safe margin exists
   xTaskCreatePinnedToCore(myTask, "MyTask", 4096, NULL, 1, NULL, 1);
   ```

### Task 5: Debugging Network Issues

**Enable comprehensive network logging:**

```cpp
// In setup()
setLogLevel(LOG_DEBUG);  // Or LOG_VERBOSE for more detail

// Network-specific logs
LOG_NET_INFO("Current network: %s", currentNetwork == WIFI ? "WiFi" : "Ethernet");
LOG_NET_DEBUG("WiFi RSSI: %d dBm", WiFi.RSSI());
LOG_NET_DEBUG("Ethernet link status: %s", Ethernet.linkStatus() == LinkON ? "UP" : "DOWN");
```

**Check NetworkHysteresis status:**

```cpp
NetworkHysteresis *hysteresis = NetworkHysteresis::getInstance();
bool canSwitch = hysteresis->canSwitchNetwork();
LOG_NET_DEBUG("Can switch network: %s", canSwitch ? "YES" : "NO");
LOG_NET_DEBUG("Time until next switch: %lums",
              hysteresis->getTimeUntilNextSwitch());
```

**Force network failover (testing only):**

```cpp
NetworkMgr *netMgr = NetworkMgr::getInstance();
netMgr->forceFailover();  // Switch to backup network
```

---

## 🏗️ Build and Deployment

### Arduino IDE Configuration

**Board Settings:**
```
Tools → Board → ESP32 Arduino → ESP32-S3 Dev Module
Tools → Flash Size → 16MB (128Mb)
Tools → PSRAM → OPI PSRAM
Tools → Partition Scheme → Default 4MB with SPIFFS
Tools → Upload Speed → 921600
Tools → USB CDC On Boot → Enabled
Tools → CPU Frequency → 240MHz (WiFi/BT)
Tools → Core Debug Level → None (production) or Info (development)
```

### Required Arduino Libraries

**Install via Library Manager (Tools → Manage Libraries):**

| Library           | Version | Author               | Purpose                   |
|-------------------|---------|----------------------|---------------------------|
| ArduinoJson       | 7.4.2+  | Benoit Blanchon      | JSON parsing/serialization |
| RTClib            | 2.1.4+  | Adafruit             | DS3231 RTC interface      |
| NTPClient         | 3.2.1+  | Fabrice Weinberg     | NTP time synchronization  |
| Ethernet          | 2.0.2+  | Arduino              | W5500 Ethernet driver     |
| TBPubSubClient    | 2.12.1+ | ThingsBoard          | MQTT client               |
| ModbusMaster      | 2.0.1+  | Doc Walker           | Modbus RTU master         |
| OneButton         | 2.0+    | Matthias Hertel      | Button debouncing         |
| ArduinoHttpClient | 0.6.1+  | Arduino              | HTTP/HTTPS client         |

**See `/Documentation/Technical_Guides/LIBRARIES.md` for detailed installation instructions.**

### Compilation

**Development Build:**
```cpp
#define PRODUCTION_MODE 0
```
- Full logging enabled
- BLE always on
- Serial debug output
- ~2.1MB firmware size

**Production Build:**
```cpp
#define PRODUCTION_MODE 1
```
- Minimal logging (ERROR + WARN)
- BLE button-controlled
- No debug output
- ~1.8MB firmware size (15% smaller)

### Upload Process

1. **Connect ESP32-S3** via USB-C
2. **Select port** (Tools → Port)
3. **Click Upload** (Ctrl+U / Cmd+U)
4. **Wait** for "Done uploading" (~30 seconds @ 921600 baud)
5. **Open Serial Monitor** (115200 baud)
6. **Verify boot messages:**
   ```
   [INFO] System initialization...
   [INFO] PSRAM: 8388608 bytes
   [INFO] Free DRAM: 285432 bytes
   [INFO] ConfigManager initialized
   [INFO] BLE advertising started: SURIOTA GW
   [INFO] Network initialized (Ethernet primary)
   [INFO] Modbus RTU service started
   [INFO] System ready
   ```

### Deployment Checklist

**Before deployment:**
- [ ] Set `PRODUCTION_MODE = 1`
- [ ] Configure WiFi credentials in `/network_config.json`
- [ ] Configure MQTT broker in `/server_config.json`
- [ ] Test BLE button control (long-press to enable)
- [ ] Test network failover (disconnect Ethernet)
- [ ] Test Modbus communication with actual devices
- [ ] Verify LED indicators (NET, STATUS)
- [ ] Run for 24 hours (stability test)
- [ ] Check memory usage (no leaks)
- [ ] Document device configurations
- [ ] Create backup of SD card files

**After deployment:**
- [ ] Monitor Serial logs (if accessible)
- [ ] Monitor MQTT broker for data
- [ ] Check LED NET status (solid = connected)
- [ ] Verify device polling intervals
- [ ] Test configuration changes via BLE
- [ ] Document any issues in `/Documentation/Technical_Guides/TROUBLESHOOTING.md`

---

## ⚠️ Important Warnings

### 1. CRITICAL: DebugConfig.h Include Order

**❌ WRONG - Will cause compilation errors:**
```cpp
#include "BLEManager.h"
#include "DebugConfig.h"  // Too late!
```

**✅ CORRECT - Always first:**
```cpp
#include "DebugConfig.h"        // ← MUST BE FIRST
#include "MemoryRecovery.h"     // Phase 2 optimization
#include "BLEManager.h"
```

**Reason:** `DebugConfig.h` defines all `LOG_*` macros. Including it after other headers will cause "undefined macro" errors.

### 2. PSRAM Allocation Failure Handling

**Always provide heap fallback:**

```cpp
// ✅ CORRECT - With fallback
void* ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
if (ptr) {
    obj = new (ptr) MyClass();
} else {
    obj = new MyClass();  // Fallback to heap
}

// ❌ WRONG - No fallback
void* ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
obj = new (ptr) MyClass();  // Crash if PSRAM allocation fails!
```

### 3. Thread Safety in FreeRTOS

**Always protect shared resources:**

```cpp
// ✅ CORRECT - Mutex protected
xSemaphoreTake(cacheMutex, portMAX_DELAY);
cacheDevices["devices"] = doc;
xSemaphoreGive(cacheMutex);

// ❌ WRONG - Race condition
cacheDevices["devices"] = doc;  // Multiple tasks accessing!
```

### 4. Atomic File Operations

**Always use atomic writes for critical config:**

```cpp
// ✅ CORRECT - Atomic write
bool ConfigManager::saveConfig(JsonDocument &doc) {
    return atomicWrite("/devices.json", doc);  // WAL protection
}

// ❌ WRONG - Direct write (corruption risk)
bool ConfigManager::saveConfig(JsonDocument &doc) {
    File file = LittleFS.open("/devices.json", "w");
    serializeJson(doc, file);
    file.close();
    return true;  // Crash during write = corrupted file!
}
```

### 5. BLE Response Format (v2.1.1+)

**Always return full objects in CRUD responses:**

```cpp
// ✅ CORRECT - Full object in response
{
    "status": "ok",
    "device_id": "D7A3F2",
    "data": {                    // ← Complete object
        "device_name": "Sensor",
        "protocol": "RTU",
        "registers": [...]
    }
}

// ❌ WRONG - No data field (mobile app needs additional READ)
{
    "status": "ok",
    "device_id": "D7A3F2"
}
```

### 6. Configuration Change Notifications

**Always notify services after config changes:**

```cpp
// ✅ CORRECT - Notify affected services
bool ConfigManager::updateDevice(JsonDocument &device) {
    if (saveDevicesConfig(devices)) {
        modbusRtuService->notifyConfigChange();  // ← REQUIRED
        modbusTcpService->notifyConfigChange();  // ← REQUIRED
        return true;
    }
    return false;
}

// ❌ WRONG - No notification (services use stale config)
bool ConfigManager::updateDevice(JsonDocument &device) {
    return saveDevicesConfig(devices);  // Services won't refresh!
}
```

### 7. Network Failover Hysteresis

**Respect network switching delays:**

```cpp
// ✅ CORRECT - Check before switching
NetworkHysteresis *hysteresis = NetworkHysteresis::getInstance();
if (hysteresis->canSwitchNetwork()) {
    networkManager->switchToBackup();
}

// ❌ WRONG - Rapid switching causes instability
if (currentNetworkDown) {
    networkManager->switchToBackup();  // May oscillate!
}
```

### 8. Serial Baudrate Switching

**Always restore original baudrate:**

```cpp
// ✅ CORRECT - Restore after polling
void ModbusRtuService::pollDevice(Device &device) {
    if (device.baud_rate != currentBaudRate) {
        Serial1.updateBaudRate(device.baud_rate);
        currentBaudRate = device.baud_rate;
        vTaskDelay(pdMS_TO_TICKS(100));  // Stabilization delay
    }

    // ... poll device ...

    // Baudrate automatically changed for next device in loop
}

// ❌ WRONG - Baudrate mismatch
void ModbusRtuService::pollDevice(Device &device) {
    // Assumes all devices use same baudrate!
    modbus.readHoldingRegisters(address, quantity);
}
```

### 9. JSON Document Sizing

**Always allocate sufficient size:**

```cpp
// ✅ CORRECT - Adequate size with margin
SpiRamJsonDocument doc(8192);  // 8KB for ~100 registers

// ❌ WRONG - Undersized (silent truncation)
JsonDocument doc(512);  // Only 512 bytes!
serializeJson(largeObject, doc);  // Data loss!
```

**Use `measureJson()` to determine size:**
```cpp
size_t size = measureJson(doc);
LOG_CONFIG_DEBUG("JSON size: %d bytes", size);
```

### 10. Task Stack Overflow

**Monitor stack high water mark:**

```cpp
// In task function
UBaseType_t stackRemaining = uxTaskGetStackHighWaterMark(NULL);
if (stackRemaining < 500) {  // Less than 500 bytes free
    LOG_MEM_WARN("Task stack low: %d bytes remaining", stackRemaining);
}

// Increase stack size if needed
xTaskCreatePinnedToCore(myTask, "MyTask", 16384, NULL, 1, NULL, 1);
//                                         ^^^^^ Increase this
```

### 11. String Memory Management

**Use String carefully in embedded systems:**

```cpp
// ✅ BETTER - Use const char* when possible
void processData(const char *deviceId) {
    LOG_RTU_INFO("Processing device: %s", deviceId);
}

// ⚠️ CAREFUL - String causes heap fragmentation
void processData(const String &deviceId) {
    String message = "Processing " + deviceId;  // Heap allocation
}

// ✅ BEST - Pre-allocate buffer
char buffer[128];
snprintf(buffer, sizeof(buffer), "Processing %s", deviceId);
```

### 12. Watchdog Timer

**Feed watchdog in long-running tasks:**

```cpp
// Long polling loop
void modbusTask(void *parameter) {
    while (true) {
        for (auto &device : devices) {
            pollDevice(device);

            // Feed watchdog every iteration
            vTaskDelay(pdMS_TO_TICKS(10));  // Yield to IDLE task
        }
    }
}
```

---

## 📚 Additional Resources

### Documentation Files

**Primary Documentation:**
- `/README.md` - Project overview and quick start
- `/Documentation/Changelog/VERSION_HISTORY.md` - Version history (MUST UPDATE)
- `/Documentation/API_Reference/API.md` - BLE CRUD API reference

**Technical Guides:**
- `/Documentation/Technical_Guides/MODBUS_DATATYPES.md` - 40+ Modbus data types
- `/Documentation/Technical_Guides/PROTOCOL.md` - BLE/Modbus protocol details
- `/Documentation/Technical_Guides/LOGGING.md` - Debug log reference
- `/Documentation/Technical_Guides/LIBRARIES.md` - Arduino library setup
- `/Documentation/Technical_Guides/HARDWARE.md` - GPIO pinout, schematics

**Advanced Topics:**
- `/Documentation/Technical_Guides/MQTT_PUBLISH_MODES_DOCUMENTATION.md` - MQTT modes
- `/Documentation/Technical_Guides/REGISTER_CALIBRATION_DOCUMENTATION.md` - Calibration
- `/Documentation/Technical_Guides/CAPACITY_ANALYSIS.md` - Performance limits
- `/Documentation/Technical_Guides/TROUBLESHOOTING.md` - Common issues

### External Resources

**ESP32-S3 Documentation:**
- [ESP32-S3 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf)
- [ESP32-S3 Technical Reference Manual](https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_en.pdf)
- [Arduino-ESP32 Documentation](https://docs.espressif.com/projects/arduino-esp32/en/latest/)

**Modbus Protocol:**
- [Modbus Application Protocol Specification](https://modbus.org/docs/Modbus_Application_Protocol_V1_1b3.pdf)
- [Modbus over Serial Line Specification](https://modbus.org/docs/Modbus_over_serial_line_V1_02.pdf)

**FreeRTOS:**
- [FreeRTOS Documentation](https://www.freertos.org/Documentation/RTOS_book.html)
- [ESP-IDF FreeRTOS](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/freertos.html)

---

## 🆘 Getting Help

### Debugging Steps

1. **Check Serial Monitor** (115200 baud) for error messages
2. **Enable verbose logging:**
   ```cpp
   setLogLevel(LOG_VERBOSE);
   ```
3. **Check memory usage:**
   ```cpp
   LOG_MEM_INFO("Free DRAM: %d", heap_caps_get_free_size(MALLOC_CAP_8BIT));
   LOG_MEM_INFO("Free PSRAM: %d", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
   ```
4. **Check network status:**
   ```cpp
   LOG_NET_INFO("WiFi: %s, Ethernet: %s",
                WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected",
                Ethernet.linkStatus() == LinkON ? "UP" : "DOWN");
   ```
5. **Check configuration files:**
   ```cpp
   if (!LittleFS.exists("/devices.json")) {
       LOG_CONFIG_ERROR("devices.json not found!");
   }
   ```
6. **Consult `/Documentation/Technical_Guides/TROUBLESHOOTING.md`**

### Contact Information

**SURIOTA R&D Team:**
- **Email:** support@suriota.com
- **Website:** www.suriota.com
- **GitHub:** https://github.com/suriota/SRT-MGATE-1210-Firmware

**Developer:** Kemal
**Last Updated:** November 20, 2025

---

## 📝 Version History

| Version | Date       | Changes                                                                                          |
|---------|------------|--------------------------------------------------------------------------------------------------|
| 2.0.0   | 2025-11-20 | Major update with advanced features, optimization phases, troubleshooting examples               |
| 1.0.0   | 2025-11-20 | Initial CLAUDE.md creation                                                                       |

### Version 2.0.0 Changes

**Added:**
- ✅ **Advanced Features & Optimization** section with optimization phases (Phase 1-4, v2.1.1, v2.2.0)
- ✅ **Priority Queue & Batch Operations** documentation with examples
- ✅ **Log Throttling** guide to prevent log spam
- ✅ **BLE Metrics & Performance Monitoring** complete reference
- ✅ **RTC Timestamps** (Phase 3) integration guide
- ✅ **MTU Negotiation State Machine** detailed explanation
- ✅ **7 Comprehensive Troubleshooting Examples** with diagnosis steps:
  - BLE connection hangs during MTU negotiation
  - Memory exhaustion with large device configurations
  - Modbus RTU device not responding
  - Network failover oscillation
  - MQTT messages not publishing
  - High priority commands not executing first
  - Log spam from failing devices

**Improved:**
- Updated with recent v2.2.0 changes (HTTP interval refactoring)
- Added practical code examples for all advanced features
- Enhanced troubleshooting with root cause analysis
- Added prevention strategies for common issues

---

**Made with ❤️ by SURIOTA R&D Team**
*Empowering Industrial IoT Solutions*

---

## 🔍 Troubleshooting Examples

### Example 1: BLE Connection Hangs During MTU Negotiation

**Symptom:**
- Mobile app shows "Connecting..." indefinitely
- No response from gateway after 30+ seconds
- Serial Monitor shows: `[BLE] MTU negotiation started...` but never completes

**Root Cause:**
MTU negotiation timeout (fixed in Phase 4 optimization)

**Solution:**
```cpp
// Check MTU negotiation status
MTUMetrics metrics = bleManager->getMTUMetrics();

if (metrics.timeoutCount > 0) {
    LOG_BLE_WARN("MTU negotiation timed out %d times", metrics.timeoutCount);
    LOG_BLE_INFO("Using fallback MTU: %d bytes", metrics.mtuSize);
}

// If using older firmware, update to v2.2.0+ which includes Phase 4 fixes
```

**Prevention:**
- Firmware v2.2.0+ automatically handles MTU timeouts
- Falls back to 100-byte MTU after 2 retries (~15s total)
- Mobile app continues with minimal MTU (slower but functional)

### Example 2: Memory Exhaustion During Large Device Configuration

**Symptom:**
- Gateway crashes when loading 50+ devices
- Serial Monitor shows: `[MEM] CRITICAL: Free DRAM below threshold`
- System restarts unexpectedly

**Root Cause:**
Large JSON documents allocated in DRAM instead of PSRAM

**Solution:**
```cpp
// Check memory allocation
void diagnoseMemory() {
    size_t dramFree = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t psramFree = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

    LOG_MEM_INFO("DRAM free: %d bytes", dramFree);
    LOG_MEM_INFO("PSRAM free: %d bytes", psramFree);

    if (dramFree < 50000) {
        LOG_MEM_WARN("DRAM critically low! Moving allocations to PSRAM...");

        // Force cache clear
        configManager->refreshCache();

        // Re-allocate large objects in PSRAM
        // ConfigManager should already be in PSRAM (see Main.ino)
    }
}

// Verify ConfigManager is in PSRAM
void* cmPtr = (void*)configManager;
if (heap_caps_get_allocated_size(cmPtr) == 0) {
    LOG_MEM_ERROR("ConfigManager NOT in PSRAM!");
    // Restart required to re-allocate properly
}
```

**Prevention:**
- Always use `SpiRamJsonDocument` for large JSON docs
- Monitor memory with `MemoryRecovery::logMemoryStatus()`
- Set appropriate thresholds in setup()

### Example 3: Modbus RTU Device Not Responding

**Symptom:**
- Device configured correctly but no data
- Serial Monitor shows: `[RTU] Device XXXXX read failed. Retry 1/3`
- Device eventually disabled after max retries

**Diagnosis Steps:**
```cpp
// 1. Check device configuration
JsonDocument deviceDoc;
configManager->readDevice("XXXXX", deviceDoc);

LOG_RTU_DEBUG("Device config:");
LOG_RTU_DEBUG("  Slave ID: %d", deviceDoc["slave_id"].as<int>());
LOG_RTU_DEBUG("  Baud rate: %d", deviceDoc["baud_rate"].as<int>());
LOG_RTU_DEBUG("  Serial port: %d", deviceDoc["serial_port"].as<int>());
LOG_RTU_DEBUG("  Timeout: %d ms", deviceDoc["timeout"].as<int>());

// 2. Check baudrate switching
void ModbusRtuService::debugBaudrate() {
    LOG_RTU_DEBUG("Current baudrate: %d", currentBaudRate);
    LOG_RTU_DEBUG("Target baudrate: %d", device.baud_rate);

    if (currentBaudRate != device.baud_rate) {
        LOG_RTU_INFO("Switching baudrate: %d → %d",
                     currentBaudRate, device.baud_rate);
    }
}

// 3. Check hardware connections
void testRS485Hardware() {
    // Send test frame
    Serial1.begin(9600, SERIAL_8N1, RTU1_RX, RTU1_TX);
    Serial1.write(testFrame, sizeof(testFrame));

    delay(100);

    if (Serial1.available()) {
        LOG_RTU_INFO("RS485 hardware responding");
    } else {
        LOG_RTU_ERROR("RS485 hardware NOT responding - check wiring");
    }
}
```

**Common Issues:**
- **Wrong baudrate:** Ensure device baudrate matches gateway configuration
- **Incorrect wiring:** Check A/B polarity on RS485
- **Termination resistor:** Required on long cables (120Ω)
- **Slave ID mismatch:** Verify device actual slave ID
- **Timeout too short:** Increase timeout for slow devices

### Example 4: Network Failover Oscillation

**Symptom:**
- Network constantly switching between Ethernet and WiFi
- Serial Monitor shows rapid network change messages
- MQTT disconnects frequently

**Root Cause:**
Hysteresis not enabled or too short

**Solution:**
```cpp
// Check hysteresis configuration
NetworkHysteresis *hysteresis = NetworkHysteresis::getInstance();

bool enabled = hysteresis->isEnabled();
uint32_t delayMs = hysteresis->getHysteresisDelay();
bool canSwitch = hysteresis->canSwitchNetwork();

LOG_NET_DEBUG("Hysteresis enabled: %s", enabled ? "YES" : "NO");
LOG_NET_DEBUG("Hysteresis delay: %lu ms", delayMs);
LOG_NET_DEBUG("Can switch now: %s", canSwitch ? "YES" : "NO");

// Fix: Enable hysteresis with longer delay
if (!enabled) {
    hysteresis->setEnabled(true);
    hysteresis->setHysteresisDelay(15000);  // 15 seconds minimum
    LOG_NET_INFO("Hysteresis enabled with 15s delay");
}

// Check network stability
unsigned long timeSinceSwitch = hysteresis->getTimeSinceLastSwitch();
LOG_NET_INFO("Time since last switch: %lu ms", timeSinceSwitch);
```

**Prevention:**
- Enable network hysteresis in `/network_config.json`
- Set `hysteresis_ms` to at least 10000 (10 seconds)
- Monitor network quality before allowing failover

### Example 5: MQTT Messages Not Publishing

**Symptom:**
- Devices polling successfully
- Queue shows data being added
- But no messages appear on MQTT broker

**Diagnosis:**
```cpp
// 1. Check MQTT connection status
bool connected = mqttManager->isConnected();
LOG_MQTT_INFO("MQTT connected: %s", connected ? "YES" : "NO");

if (!connected) {
    String broker = serverConfig->getMqttBrokerAddress();
    int port = serverConfig->getMqttBrokerPort();
    LOG_MQTT_ERROR("Not connected to broker %s:%d", broker.c_str(), port);

    // Check network
    if (!networkManager->isConnected()) {
        LOG_MQTT_ERROR("Network not connected - MQTT cannot connect");
    }
}

// 2. Check queue status
QueueManager *queueMgr = QueueManager::getInstance();
int queueSize = queueMgr->getSize();
int queueCapacity = queueMgr->getCapacity();

LOG_QUEUE_INFO("Queue: %d/%d items", queueSize, queueCapacity);

if (queueSize > queueCapacity * 0.9) {
    LOG_QUEUE_WARN("Queue near capacity! Possible publish bottleneck");
}

// 3. Check publish mode configuration
JsonDocument serverConfigDoc = serverConfig->getConfig();
String protocol = serverConfigDoc["protocol"].as<String>();

if (protocol != "mqtt") {
    LOG_MQTT_ERROR("Protocol is '%s', not 'mqtt'!", protocol.c_str());
    LOG_MQTT_INFO("Update server_config to use MQTT protocol");
}

// 4. Test manual publish
bool published = mqttManager->publish("test/topic", "{\"test\":\"data\"}");
LOG_MQTT_INFO("Manual publish result: %s", published ? "SUCCESS" : "FAILED");
```

**Common Issues:**
- Protocol set to "http" instead of "mqtt"
- Wrong broker address/port
- Network connectivity issues
- Broker authentication required but not configured
- QoS level too high for broker capabilities

### Example 6: High Priority Command Not Executing First

**Symptom:**
- Emergency command sent with high priority
- But normal priority commands execute first

**Diagnosis:**
```cpp
// Check command priority in CRUDHandler
BatchStats stats = crudHandler->getBatchStats();

LOG_CRUD_INFO("Command queue statistics:");
LOG_CRUD_INFO("  High priority: %d commands", stats.highPriorityCount);
LOG_CRUD_INFO("  Normal priority: %d commands", stats.normalPriorityCount);
LOG_CRUD_INFO("  Low priority: %d commands", stats.lowPriorityCount);
LOG_CRUD_INFO("  Current queue depth: %d", stats.currentQueueDepth);
LOG_CRUD_INFO("  Peak queue depth: %d", stats.queuePeakDepth);

// Verify command includes priority field
```

**Solution:**
```json
{
  "op": "update",
  "type": "server_config",
  "priority": "high",  // ← MUST include this field
  "config": {
    "protocol": "mqtt"
  }
}
```

**Priority Execution Order:**
1. HIGH priority commands execute first (priority = 0)
2. NORMAL priority commands execute next (priority = 1, default)
3. LOW priority commands execute last (priority = 2)
4. Within same priority: FIFO (first in, first out)

### Example 7: Log Spam From Failing Device

**Symptom:**
- Serial Monitor flooded with repeated error messages
- Difficult to see other important logs
- Example: `[RTU] Device X timeout` repeated hundreds of times

**Solution - Use Log Throttling:**
```cpp
// In ModbusRtuService.cpp
LogThrottle deviceErrorThrottle(30000);  // 30 second interval

void ModbusRtuService::pollDevice(Device &device) {
    uint8_t result = modbus.readHoldingRegisters(address, quantity);

    if (result != modbus.ku8MBSuccess) {
        // Only log once every 30 seconds
        if (deviceErrorThrottle.shouldLog()) {
            LOG_RTU_ERROR("Device %s timeout (Slave:%d Port:%d)",
                          device.device_id.c_str(),
                          device.slave_id,
                          device.serial_port);
            // Will also log: "(suppressed N similar messages)"
        }

        handleDeviceFailure(device.device_id);
        return;
    }

    // Success - reset throttle
    deviceErrorThrottle.reset();
}
```

**Result:**
- Error logs once every 30 seconds instead of every poll cycle
- Shows count of suppressed messages
- Other log messages remain visible

---

## 🎯 Quick Reference for AI Assistants

### Essential Rules

1. **Always include `DebugConfig.h` first** in any file that uses logging
2. **Always use PSRAM allocation with heap fallback** for large objects
3. **Always use atomic writes** for configuration files
4. **Always notify services** after configuration changes
5. **Always return full objects** in BLE CRUD responses (v2.1.1+)
6. **Always protect shared resources** with mutexes in FreeRTOS
7. **Always update VERSION_HISTORY.md** for any changes
8. **Always respect network hysteresis** delays
9. **Always check stack high water mark** in new tasks
10. **Always test in both development and production modes**

### Common File Locations

- **Entry point:** `/Main/Main.ino`
- **Config files:** `/devices.json`, `/server_config.json`, `/network_config.json`
- **Logging system:** `/Main/DebugConfig.h`, `/Main/LoggingConfig.h`
- **Error codes:** `/Main/UnifiedErrorCodes.h`
- **Memory management:** `/Main/MemoryRecovery.h`, `/Main/PSRAMValidator.h`
- **Version history:** `/Documentation/Changelog/VERSION_HISTORY.md`
- **API reference:** `/Documentation/API_Reference/API.md`

### Code Templates

**Creating a new manager class:**
```cpp
// MyManager.h
#ifndef MY_MANAGER_H
#define MY_MANAGER_H

class MyManager {
private:
    static MyManager *instance;
    MyManager();  // Private constructor

public:
    static MyManager *getInstance();

    // Delete copy
    MyManager(const MyManager&) = delete;
    MyManager& operator=(const MyManager&) = delete;
};

#endif

// MyManager.cpp
#include "MyManager.h"
MyManager *MyManager::instance = nullptr;
```

**Creating a FreeRTOS task:**
```cpp
void myTask(void *parameter) {
    while (true) {
        // Task logic here

        vTaskDelay(pdMS_TO_TICKS(1000));  // 1 second delay
    }
}

// In setup()
xTaskCreatePinnedToCore(
    myTask,        // Function
    "MyTask",      // Name
    8192,          // Stack size
    NULL,          // Parameters
    1,             // Priority
    NULL,          // Task handle
    1              // Core (0 or 1)
);
```

**Atomic configuration write:**
```cpp
bool ConfigManager::updateConfig(JsonDocument &doc) {
    // 1. Write to WAL
    String walPath = "/config.json.wal";
    File wal = LittleFS.open(walPath.c_str(), "w");
    serializeJson(doc, wal);
    wal.close();

    // 2. Validate WAL
    File validate = LittleFS.open(walPath.c_str(), "r");
    JsonDocument validateDoc;
    if (deserializeJson(validateDoc, validate) != DeserializationError::Ok) {
        validate.close();
        return false;
    }
    validate.close();

    // 3. Atomic rename
    if (LittleFS.rename(walPath.c_str(), "/config.json")) {
        notifyConfigChange();
        return true;
    }
    return false;
}
```

---

**End of CLAUDE.md**
