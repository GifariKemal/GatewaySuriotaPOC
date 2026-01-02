# 🐛 Bug Analysis Report

> **⚠️ ARCHIVED - October 2024**
>
> This document is archived and may contain outdated bug information.
>
> **Archived From:** v2.0.0 development and bug analysis
>
> **Reason:** Most bugs have been resolved in v2.1.0+ releases
>
> **Current Documentation:** See
> [TROUBLESHOOTING.md](../Technical_Guides/TROUBLESHOOTING.md) for current issue
> diagnostics and [VERSION_HISTORY.md](VERSION_HISTORY.md) for resolved bugs
>
> **Archive Info:** See [Archive/ARCHIVE_INFO.md](../Archive/ARCHIVE_INFO.md)
> for more information
>
> **Note:** Bug status marked as "CONFIRMED" may have been fixed in subsequent
> versions. Check VERSION_HISTORY.md for fix status.

---

**SRT-MGATE-1210 Modbus IIoT Gateway - Firmware v2.0.0**

Analisis mendalam terhadap bug kritis dan potensi masalah dalam firmware.

---

## 📋 Executive Summary

| Bug ID      | Severity        | Component        | Status              | Impact                    |
| ----------- | --------------- | ---------------- | ------------------- | ------------------------- |
| **BUG-001** | 🔴 **CRITICAL** | NetworkManager   | ✅ **CONFIRMED**    | Total network failure     |
| **BUG-002** | 🔴 **CRITICAL** | Main.ino         | ✅ **CONFIRMED**    | Memory corruption / crash |
| **BUG-003** | 🟠 **HIGH**     | ConfigManager    | ⚠️ **PARTIAL**      | Data loss on corrupt file |
| **BUG-004** | 🟡 **MEDIUM**   | ModbusRtuService | ✅ **CONFIRMED**    | CPU waste / inefficiency  |
| **BUG-005** | 🟡 **MEDIUM**   | RTCManager       | ⚠️ **DESIGN ISSUE** | Potential time conflict   |
| **BUG-006** | 🟢 **LOW**      | HttpManager      | ✅ **CONFIRMED**    | Design inconsistency      |

**Priority**: Fix BUG-001 immediately (blocks all network functionality)

---

## 🔴 BUG-001: CRITICAL - NetworkManager Configuration Read Error

### Status

✅ **CONFIRMED** - Verified in code at `NetworkManager.cpp:65-82`

### Description

`NetworkManager` gagal membaca konfigurasi WiFi dan Ethernet karena mencari di
lokasi yang salah dalam struktur JSON.

### Root Cause

**Actual JSON Structure** (from `ServerConfig.cpp:68-82`):

```json
{
  "communication": {
    "primary_network_mode": "ETH",
    "wifi": {
      "enabled": true,
      "ssid": "MyWiFiNetwork",
      "password": "MySecretPassword"
    },
    "ethernet": {
      "enabled": true,
      "use_dhcp": true,
      "static_ip": "192.168.1.177",
      "gateway": "192.168.1.1",
      "subnet": "255.255.255.0"
    }
  },
  "protocol": "mqtt",
  ...
}
```

**Current Code** (NetworkManager.cpp:65-82):

```cpp
// ❌ WRONG - Reading from root level
if (serverRoot["wifi"]) {
    wifiConfig = serverRoot["wifi"].as<JsonObject>();
    wifiConfigPresent = true;
}

if (serverRoot["ethernet"]) {
    ethernetConfig = serverRoot["ethernet"].as<JsonObject>();
    ethernetConfigPresent = true;
}
```

**Result**:

- `wifiConfigPresent` = `false`
- `ethernetConfigPresent` = `false`
- `wifiEnabled` = `false`
- `ethernetEnabled` = `false`
- `activeMode` = `"NONE"`
- **Gateway NEVER connects to network!**

### Impact

- ⚠️ **Total network failure**
- ⚠️ Gateway cannot communicate with MQTT broker
- ⚠️ Gateway cannot send HTTP requests
- ⚠️ Modbus TCP completely non-functional
- ⚠️ No cloud connectivity whatsoever

### Affected Code Locations

- `NetworkManager.cpp` line 65-82
- `NetworkManager.cpp::init()` function

### Fix Required

**Option 1: Read from correct location** (Recommended)

```cpp
// ✅ CORRECT - Read from communication object
JsonObject commConfig = serverRoot["communication"].as<JsonObject>();

bool wifiConfigPresent = false;
JsonObject wifiConfig;
if (commConfig["wifi"]) {
    wifiConfig = commConfig["wifi"].as<JsonObject>();
    wifiConfigPresent = true;
    Serial.println("[NetworkMgr] Found 'wifi' config inside 'communication'.");
} else {
    Serial.println("[NetworkMgr] WARNING: 'wifi' config not found.");
}
bool wifiEnabled = wifiConfigPresent && (wifiConfig["enabled"] | false);

bool ethernetConfigPresent = false;
JsonObject ethernetConfig;
if (commConfig["ethernet"]) {
    ethernetConfig = commConfig["ethernet"].as<JsonObject>();
    ethernetConfigPresent = true;
    Serial.println("[NetworkMgr] Found 'ethernet' config inside 'communication'.");
} else {
    Serial.println("[NetworkMgr] WARNING: 'ethernet' config not found.");
}
bool ethernetEnabled = ethernetConfigPresent && (ethernetConfig["enabled"] | false);
```

**Option 2: Use existing getter methods** (Better architecture)

```cpp
// Use ServerConfig getter methods instead of direct JSON access
JsonObject wifiConfig;
bool wifiConfigPresent = serverConfig->getWifiConfig(wifiConfig);
bool wifiEnabled = wifiConfigPresent && (wifiConfig["enabled"] | false);

JsonObject ethernetConfig;
bool ethernetConfigPresent = serverConfig->getEthernetConfig(ethernetConfig);
bool ethernetEnabled = ethernetConfigPresent && (ethernetConfig["enabled"] | false);
```

### Verification

After fix, verify with serial output:

```
[NetworkMgr] Found 'wifi' config inside 'communication'.
[NetworkMgr] Found 'ethernet' config inside 'communication'.
[NetworkMgr] Initial active network: ETH. IP: 192.168.1.177
```

---

## 🔴 BUG-002: CRITICAL - Memory Deallocation Inconsistency

### Status

✅ **CONFIRMED** - Verified in code at `Main.ino:55-80` and `Main.ino:112-334`

### Description

Fungsi `cleanup()` selalu menggunakan `heap_caps_free()` untuk objek yang
mungkin dialokasi dengan `new` standard, menyebabkan undefined behavior atau
crash.

### Root Cause

**Mixed Allocation Strategy**:

```cpp
// Allocation attempt (PSRAM with fallback)
configManager = (ConfigManager *)heap_caps_malloc(sizeof(ConfigManager),
                                                   MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
if (configManager) {
    new (configManager) ConfigManager();  // Placement new in PSRAM
} else {
    configManager = new ConfigManager();  // ❌ Fallback to internal RAM (standard new)
}
```

**Cleanup Always Uses heap_caps_free**:

```cpp
void cleanup() {
    if (configManager) {
        configManager->~ConfigManager();
        heap_caps_free(configManager);  // ❌ WRONG if allocated with 'new'
    }
}
```

### Impact

- ⚠️ **Undefined behavior** if PSRAM allocation fails
- ⚠️ **Potential crash** when `cleanup()` is called
- ⚠️ **Memory corruption** in heap
- ⚠️ Affects: `ConfigManager`, `CRUDHandler`, `BLEManager`

### Affected Objects

1. `configManager` (Main.ino:112-125)
2. `crudHandler` (Main.ino:231-246)
3. `bleManager` (Main.ino:319-334)

### Fix Required

**Solution 1: Track allocation method** (Quick fix)

```cpp
// Add flags
bool configManagerInPSRAM = false;
bool crudHandlerInPSRAM = false;
bool bleManagerInPSRAM = false;

// During allocation
configManager = (ConfigManager *)heap_caps_malloc(...);
if (configManager) {
    new (configManager) ConfigManager();
    configManagerInPSRAM = true;  // ✅ Track allocation method
} else {
    configManager = new ConfigManager();
    configManagerInPSRAM = false;
}

// In cleanup()
void cleanup() {
    if (configManager) {
        configManager->~ConfigManager();
        if (configManagerInPSRAM) {
            heap_caps_free(configManager);  // PSRAM allocation
        } else {
            free(configManager);  // Standard allocation
        }
    }
}
```

**Solution 2: Always use delete** (Cleaner, requires operator overload)

```cpp
// Overload new/delete operators globally or per-class
void* operator new(size_t size) {
    void* ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!ptr) {
        ptr = malloc(size);  // Fallback
    }
    return ptr;
}

void operator delete(void* ptr) {
    heap_caps_free(ptr);  // Works for both PSRAM and DRAM
}

// Then use consistently
configManager = new ConfigManager();  // Always uses overloaded new

// Cleanup
delete configManager;  // Always uses overloaded delete
```

**Solution 3: No fallback** (Fail fast, recommended for production)

```cpp
// Allocation without fallback - fail if PSRAM unavailable
configManager = (ConfigManager *)heap_caps_malloc(sizeof(ConfigManager),
                                                   MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
if (!configManager) {
    Serial.println("FATAL: Cannot allocate ConfigManager in PSRAM!");
    // System cannot continue without PSRAM
    ESP.restart();  // Or handle gracefully
    return;
}
new (configManager) ConfigManager();

// Cleanup always safe
void cleanup() {
    if (configManager) {
        configManager->~ConfigManager();
        heap_caps_free(configManager);  // ✅ Always correct
    }
}
```

### Recommendation

Use **Solution 3** (no fallback) because:

- ESP32-S3 with 8MB PSRAM is guaranteed
- Cleaner code without tracking flags
- Fail-fast is better than subtle corruption
- Production firmware should not run without PSRAM

---

## 🟠 BUG-003: HIGH - ConfigManager Data Loss on Parse Failure

### Status

⚠️ **PARTIAL CONFIRMATION** - Logic verified, needs runtime testing

### Description

`loadDevicesCache()` marks cache as valid even when JSON parsing fails, causing
all device configurations to appear empty.

### Root Cause

**Current Logic** (ConfigManager.cpp):

```cpp
bool ConfigManager::loadDevicesCache() {
    // ...
    if (loadJson(DEVICES_FILE, *devicesCache)) {
        devicesCacheValid = true;
        // Success path
        return true;
    }

    // ❌ PROBLEM: On parse failure
    Serial.println("ERROR: Failed to parse devices.json");
    devicesCache->clear();           // Clear cache
    devicesCacheValid = true;        // ❌ Mark as VALID!
    lastDevicesCacheTime = millis(); // ❌ Set timestamp
    return true;                     // ❌ Return success!
}
```

### Impact

- ⚠️ **Data loss** if `devices.json` is temporarily corrupted
- ⚠️ All devices appear to vanish after reboot
- ⚠️ Modbus services poll nothing
- ⚠️ User thinks configuration was deleted

### Scenarios

1. **File corruption** (power loss during write)
2. **I/O error** (flash memory issue)
3. **Invalid JSON** (manual file edit)

Result: System boots with empty device list instead of retrying

### Fix Required

```cpp
bool ConfigManager::loadDevicesCache() {
    if (xSemaphoreTake(cacheMutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
        return false;
    }

    logMemoryStats("before loadDevicesCache");

    if (loadJson(DEVICES_FILE, *devicesCache)) {
        // ✅ SUCCESS: Mark cache as valid
        devicesCacheValid = true;
        lastDevicesCacheTime = millis();

        size_t deviceCount = devicesCache->as<JsonObject>().size();
        Serial.printf("[CONFIG] Loaded %u devices into cache\n", deviceCount);

        logMemoryStats("after loadDevicesCache success");
        xSemaphoreGive(cacheMutex);
        return true;
    }

    // ✅ FAILURE: Do NOT mark cache as valid
    Serial.println("ERROR: Failed to parse devices.json. Cache NOT validated.");
    Serial.println("       Will retry on next access or cache expiry.");

    devicesCache->clear();
    // ❌ DO NOT SET: devicesCacheValid = true;
    // ❌ DO NOT SET: lastDevicesCacheTime = millis();

    logMemoryStats("after loadDevicesCache failure");
    xSemaphoreGive(cacheMutex);
    return false;  // ✅ Return failure
}
```

### Additional Improvement: Backup/Recovery

```cpp
// In ConfigManager::begin()
bool ConfigManager::begin() {
    // ... existing code ...

    // Create backup on successful parse
    if (LittleFS.exists(DEVICES_FILE)) {
        if (loadJson(DEVICES_FILE, testDoc)) {
            // JSON is valid, create backup
            LittleFS.rename(DEVICES_FILE, DEVICES_FILE ".bak");
            LittleFS.rename(DEVICES_FILE ".tmp", DEVICES_FILE);
        } else {
            // JSON is invalid, try to restore from backup
            Serial.println("[CONFIG] devices.json corrupt, attempting restore from backup");
            if (LittleFS.exists(DEVICES_FILE ".bak")) {
                LittleFS.remove(DEVICES_FILE);
                LittleFS.rename(DEVICES_FILE ".bak", DEVICES_FILE);
            }
        }
    }
}
```

---

## 🟡 BUG-004: MEDIUM - Modbus Service Not Using Priority Queue

### Status

✅ **CONFIRMED** - Dead code verified in `ModbusRtuService.cpp` and
`ModbusTcpService.cpp`

### Description

Sophisticated priority queue scheduler (`refreshDeviceList()`) exists but is
never used. Current implementation inefficiently iterates all devices every
cycle.

### Root Cause

**Existing (Unused) Code**:

```cpp
// ModbusRtuService.h - Priority queue defined
std::priority_queue<PollingTask> pollingQueue;

// ModbusRtuService.cpp - Scheduler implemented but NEVER CALLED
void ModbusRtuService::refreshDeviceList() {
    // ✅ Good code - builds priority queue
    // ❌ NEVER INVOKED
}
```

**Current Implementation** (inefficient):

```cpp
void ModbusRtuService::readRtuDevicesLoop(void *parameter) {
    while (true) {
        // ❌ Load ALL devices every cycle
        JsonDocument devices;
        configManager->listDevices(devices);

        // ❌ Iterate ALL devices just to check if they should be polled
        for (JsonPair device : devices.as<JsonObject>()) {
            if (shouldPollDevice(...)) {
                // Poll device
            }
        }

        vTaskDelay(pdMS_TO_TICKS(2000));  // Wait 2 seconds and repeat
    }
}
```

### Impact

- ⚠️ **CPU waste**: Iterates 100 devices to find 1-2 ready to poll
- ⚠️ **Memory waste**: Loads full device list every 2 seconds
- ⚠️ **Scalability issue**: Performance degrades linearly with device count
- ⚠️ **JSON parsing overhead**: Parses device list repeatedly

### Performance Analysis

| Devices | Current (iterations/sec) | With Priority Queue | CPU Savings |
| ------- | ------------------------ | ------------------- | ----------- |
| 10      | 5 (10 checks)            | 0.5 (1 check)       | 90%         |
| 100     | 50 (100 checks)          | 0.5 (1 check)       | 99%         |
| 1000    | 500 (1000 checks)        | 0.5 (1 check)       | 99.9%       |

### Fix Required

**Replace loop logic**:

```cpp
void ModbusRtuService::readRtuDevicesLoop(void *parameter) {
    ModbusRtuService *self = static_cast<ModbusRtuService*>(parameter);

    // ✅ Build initial priority queue
    self->refreshDeviceList();

    while (true) {
        if (self->pollingQueue.empty()) {
            // No devices ready, refresh queue
            self->refreshDeviceList();
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        // ✅ Get highest priority task
        PollingTask task = self->pollingQueue.top();
        self->pollingQueue.pop();

        // ✅ Check if task is ready (time-based)
        unsigned long now = millis();
        if (now < task.nextPollTime) {
            // Not ready yet, put back and wait
            self->pollingQueue.push(task);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        // ✅ Poll device
        self->pollModbusDevice(task.deviceId, task.slaveId, task.port, task.baudRate);

        // ✅ Update next poll time and re-queue
        task.nextPollTime = now + task.refreshInterval;
        self->pollingQueue.push(task);

        // Small delay to prevent tight loop
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

### Benefits After Fix

- ✅ CPU usage reduced by 90-99%
- ✅ Scalable to 1000+ devices
- ✅ Automatic priority handling
- ✅ Efficient time-based scheduling

---

## 🟡 BUG-005: MEDIUM - RTCManager Race Condition with ESP32 SNTP

### Status

⚠️ **DESIGN ISSUE** - Redundant NTP implementation

### Description

Manual NTP synchronization loop conflicts with ESP32's built-in SNTP background
task.

### Root Cause

**Current Implementation**:

```cpp
bool RTCManager::syncWithNTP() {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2, ntpServer3);
    // ↑ This starts ESP32's background SNTP task

    // ❌ Then immediately do manual sync
    struct tm timeinfo;
    unsigned long start = millis();
    while (!getLocalTime(&timeinfo)) {  // ❌ Busy-wait loop
        if (millis() - start > 30000) {
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    // ↓ Manual settimeofday call (conflicts with SNTP task)
}
```

**Problem**:

- ESP32 SNTP task runs in background
- Your manual code also tries to sync
- Both call `settimeofday()` → race condition
- Wastes CPU in busy-wait loop

### Impact

- ⚠️ Time instability (multiple sources updating)
- ⚠️ CPU waste in sync loop
- ⚠️ Redundant code (SNTP already handles this)

### Fix Required

**Solution: Use ESP32 SNTP properly**

```cpp
#include <esp_sntp.h>

bool RTCManager::init() {
    // Initialize I2C and RTC (existing code)
    // ...

    // ✅ Configure SNTP properly
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_setservername(1, "time.nist.gov");
    sntp_setservername(2, "time.google.com");

    // Set timezone for GMT+7 (WIB)
    setenv("TZ", "WIB-7", 1);
    tzset();

    // ✅ Start SNTP and wait for sync
    sntp_init();

    Serial.println("[RTC] Waiting for NTP time sync...");
    int retry = 0;
    const int max_retry = 30;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < max_retry) {
        Serial.printf("[RTC] Waiting for system time to be set... (%d/%d)\n", retry, max_retry);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    if (retry >= max_retry) {
        Serial.println("[RTC] Failed to sync with NTP");
        return false;
    }

    // ✅ Sync RTC from system time
    time_t now;
    time(&now);
    rtc.adjust(DateTime(now));

    Serial.println("[RTC] Time synchronized successfully");
    return true;
}

// ❌ Remove timeSyncLoop task - not needed
// ❌ Remove syncWithNTP - not needed
// ✅ Keep periodicSyncWithNTP for occasional re-sync
```

---

## 🟢 BUG-006: LOW - HttpManager Client Inconsistency

### Status

✅ **CONFIRMED** - Design inconsistency verified

### Description

`HttpManager` requests `activeClient` from `NetworkManager` but doesn't use it,
unlike `MqttManager` which correctly uses it.

### Root Cause

**MqttManager (Correct)**:

```cpp
bool MqttManager::connectToMqtt() {
    Client* activeClient = networkManager->getActiveClient();  // ✅ Get client
    mqttClient.setClient(*activeClient);  // ✅ Use it
    return mqttClient.connect(...);
}
```

**HttpManager (Inconsistent)**:

```cpp
bool HttpManager::sendHttpRequest(...) {
    Client* activeClient = networkManager->getActiveClient();  // Gets client
    // ❌ But never uses it!

    HTTPClient httpClient;
    httpClient.begin(endpointUrl);  // ❌ HTTPClient creates own client internally
    // ...
}
```

### Impact

- ⚠️ Design inconsistency
- ⚠️ NetworkManager's client management bypassed
- ⚠️ May cause issues if NetworkManager tracks connections
- ⚠️ Low severity - HTTP still works

### Fix Required

```cpp
bool HttpManager::sendHttpRequest(const String &endpointUrl,
                                   const String &method,
                                   const JsonDocument &data) {
    // ✅ Get active client from NetworkManager
    Client* activeClient = networkManager->getActiveClient();
    if (!activeClient) {
        Serial.println("[HTTP] ERROR: No active network client available");
        return false;
    }

    HTTPClient httpClient;

    // ✅ Pass client to HTTPClient
    httpClient.begin(*activeClient, endpointUrl);

    // Rest of code remains the same
    httpClient.addHeader("Content-Type", "application/json");
    // ...
}
```

---

## 📊 Priority Matrix

### Immediate Action Required

```
Priority 1 (CRITICAL - Fix Immediately):
├── BUG-001: NetworkManager config path  [1 hour to fix]
└── BUG-002: Memory deallocation          [2 hours to fix]

Priority 2 (HIGH - Fix This Week):
└── BUG-003: ConfigManager parse failure  [1 hour to fix]

Priority 3 (MEDIUM - Fix This Month):
├── BUG-004: Modbus priority queue        [4 hours to fix]
└── BUG-005: RTC SNTP redundancy          [2 hours to fix]

Priority 4 (LOW - Technical Debt):
└── BUG-006: HTTP client consistency      [0.5 hours to fix]
```

### Estimated Total Fix Time

- **Critical bugs**: 3 hours
- **All bugs**: 10.5 hours

---

## 🧪 Testing Checklist

After fixes, verify:

### BUG-001 Testing

```cpp
// Expected serial output:
[NetworkMgr] Found 'wifi' config inside 'communication'.
[NetworkMgr] Found 'ethernet' config inside 'communication'.
[NetworkMgr] WiFi enabled: true
[NetworkMgr] Ethernet enabled: true
[NetworkMgr] Initial active network: ETH. IP: 192.168.1.177
```

### BUG-002 Testing

```cpp
// Test PSRAM failure scenario:
1. Temporarily disable PSRAM in code
2. Trigger cleanup()
3. Verify no crash
4. Re-enable PSRAM
```

### BUG-003 Testing

```cpp
// Test corrupt file recovery:
1. Manually corrupt devices.json
2. Reboot system
3. Verify system doesn't show "0 devices"
4. Verify retry mechanism works
```

---

## 📝 Conclusion

### Bug Severity Summary

- **2 Critical bugs** blocking core functionality
- **1 High bug** causing potential data loss
- **2 Medium bugs** reducing efficiency
- **1 Low bug** causing minor inconsistency

### Impact Assessment

Without fixes:

- ❌ **Gateway is completely non-functional** (no network)
- ❌ **Risk of crashes** (memory corruption)
- ❌ **Poor performance** at scale (inefficient polling)

With fixes:

- ✅ **Full network connectivity** restored
- ✅ **Stable memory management**
- ✅ **Scalable to 1000+ devices**
- ✅ **Production-ready firmware**

### Recommendation

**Fix BUG-001 and BUG-002 immediately** before any production deployment. These
are blocking issues that prevent basic operation.

---

**Copyright © 2025 PT Surya Inovasi Prioritas (SURIOTA)**

_This document is part of the SRT-MGATE-1210 firmware technical documentation._
