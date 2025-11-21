# 🐛 Bug Status Report - November 21, 2025

**SRT-MGATE-1210 Firmware - Bug Analysis**
**Version:** v2.2.0+
**Analyst:** Claude AI | **Date:** 2025-11-21 WIB (GMT+7)

---

## 📋 Executive Summary

Analysis of 8 reported bugs with priority classification, root cause analysis, and fix recommendations.

| Bug # | Priority | Component | Status | Severity | Fix Effort |
|-------|----------|-----------|--------|----------|-----------|
| **#17** | 🔴 High | NetworkManager | ✅ **CONFIRMED** | Critical | Medium |
| **#18** | 🟡 Medium | BLEManager | ⚠️ **PARTIAL** | Medium | Low |
| **#20** | 🟡 Medium | ModbusRtuService | ✅ **FIXED** | Low | - |
| **#22** | 🟢 Low | LEDManager | ⏳ **PENDING** | Low | Low |
| **#23** | 🟢 Low | RTCManager | ⏳ **PENDING** | Low | Low |
| **#24** | 🟡 Medium | ConfigManager | ⚠️ **PARTIAL** | Medium | Low |
| **#25** | 🟢 Low | ButtonManager | ⏳ **PENDING** | Low | Low |
| **#26** | 🟢 Low | UnifiedErrorCodes | ⚠️ **PARTIAL** | Low | Low |

**Critical Findings:**
- **BUG #17** requires immediate attention (network failover non-functional)
- **BUG #20** already fixed by recent enhancement (auto-recovery + metrics tracking)
- **BUG #18** partially addressed, but needs monitoring task integration

---

## 🔴 BUG #17: Network Failover Not Working Properly

### Status
✅ **CONFIRMED** - Critical design flaw

### Severity
**CRITICAL** - High priority, medium effort to fix

### Description
Network failover mechanism exists in `NetworkManager` but is **not actively monitored**. The `switchMode()` method is implemented but never automatically triggered.

### Root Cause Analysis

**Current Implementation:**
- ✅ `NetworkManager::switchMode()` exists and is thread-safe (NetworkManager.cpp:369-431)
- ✅ `NetworkHysteresis` prevents rapid switching (works correctly)
- ❌ **NO monitoring task** to detect connection failures
- ❌ **NO automatic trigger** for failover

**Evidence:**
```bash
# Search for monitoring task or health check
$ grep -r "monitorTask\|checkHealth\|failover" Main/
# Result: No active monitoring task found
```

**File:** `Main/Main.ino`
- NetworkManager is initialized (line 279-287)
- **NO loop()** monitoring added
- **NO FreeRTOS task** created for health checking

### Impact
- ⚠️ **Manual failover only** - requires explicit API call
- ⚠️ If WiFi fails, system does NOT automatically switch to Ethernet
- ⚠️ If Ethernet fails, system does NOT automatically switch to WiFi
- ⚠️ Network downtime until manual intervention

### Recommended Fix

**Option A: Create Network Monitoring Task (Recommended)**

Add to `Main/NetworkManager.h`:
```cpp
class NetworkMgr {
private:
  static void networkMonitorTask(void *parameter);
  void networkMonitorLoop();
  TaskHandle_t monitorTaskHandle = nullptr;
  bool monitoringEnabled = false;

public:
  void startMonitoring();
  void stopMonitoring();
};
```

Add to `Main/NetworkManager.cpp`:
```cpp
void NetworkMgr::startMonitoring() {
  if (monitoringEnabled) return;

  monitoringEnabled = true;
  xTaskCreatePinnedToCore(
    networkMonitorTask,
    "NET_MONITOR",
    4096,
    this,
    1,
    &monitorTaskHandle,
    1
  );
}

void NetworkMgr::networkMonitorTask(void *parameter) {
  NetworkMgr *mgr = static_cast<NetworkMgr *>(parameter);
  mgr->networkMonitorLoop();
}

void NetworkMgr::networkMonitorLoop() {
  const unsigned long CHECK_INTERVAL_MS = 5000; // Check every 5s

  while (monitoringEnabled) {
    // Check current network health
    bool currentNetworkHealthy = false;

    if (activeMode == "WIFI") {
      currentNetworkHealthy = (WiFi.status() == WL_CONNECTED);
    } else if (activeMode == "ETH") {
      currentNetworkHealthy = (Ethernet.linkStatus() == LinkON);
    }

    // If current network unhealthy, trigger failover
    if (!currentNetworkHealthy) {
      Serial.printf("[NetworkMgr] %s unhealthy, attempting failover...\n",
                    activeMode.c_str());

      // Check hysteresis
      NetworkHysteresis *hysteresis = NetworkHysteresis::getInstance();
      if (hysteresis && hysteresis->canSwitchNetwork()) {
        // Try backup network
        String backupMode = (activeMode == "WIFI") ? "ETH" : "WIFI";
        switchMode(backupMode);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(CHECK_INTERVAL_MS));
  }
}
```

Add to `Main/Main.ino` after NetworkManager init:
```cpp
// Start network health monitoring
if (networkManager) {
  networkManager->startMonitoring();
  Serial.println("Network health monitoring started");
}
```

**Estimated Effort:** ~2 hours (implementation + testing)

---

## 🟡 BUG #18: BLE MTU Negotiation Timeout

### Status
⚠️ **PARTIAL FIX** - Code exists but not actively monitored

### Severity
**MEDIUM** - Medium priority, low effort to complete

### Description
MTU negotiation timeout handling exists (`BLEManager::checkMTUNegotiationTimeout()`) but is **not called periodically**. Timeout detection is passive.

### Root Cause Analysis

**Current Implementation:**
- ✅ `initiateMTUNegotiation()` exists (BLEManager.cpp:917-931)
- ✅ `checkMTUNegotiationTimeout()` exists (BLEManager.cpp:945-973)
- ✅ `handleMTUNegotiationTimeout()` exists (BLEManager.cpp:975-1012)
- ⚠️ `checkMTUNegotiationTimeout()` **never called** - no task monitors it

**Evidence:**
```bash
# Search for where checkMTUNegotiationTimeout is called
$ grep -n "checkMTUNegotiationTimeout" Main/BLEManager.cpp
# Result: Only definition found, no invocations
```

**Current Flow:**
1. Client connects → `onConnect()` called
2. `initiateMTUNegotiation()` starts timer (line 919)
3. Wait 200ms (line 223)
4. `logMTUNegotiation()` logs current MTU (line 226)
5. **Timeout never checked!** - if negotiation hangs, no recovery

### Impact
- ⚠️ If MTU negotiation hangs (rare), no timeout recovery
- ⚠️ System continues with default MTU (23 bytes) - works but inefficient
- ⚠️ Large responses may fragment excessively (performance hit)

### Recommended Fix

**Option A: Add Timeout Check in Metrics Task (Simple)**

Modify `Main/BLEManager.cpp` - add to metrics monitoring task:

Find the `metricsMonitorLoop()` function and add:
```cpp
void BLEManager::metricsMonitorLoop() {
  while (true) {
    // Existing queue metrics update
    updateQueueMetrics();

    // NEW: Check MTU negotiation timeout
    checkMTUNegotiationTimeout();

    vTaskDelay(pdMS_TO_TICKS(1000)); // Check every 1s
  }
}
```

**Estimated Effort:** ~15 minutes (add 1 line + test)

**Option B: Current Workaround**

The current 200ms delay + `logMTUNegotiation()` provides a practical workaround:
- Most clients negotiate within 100-200ms
- After 200ms, system logs actual MTU
- If negotiation failed, falls back to 23 bytes (works)

**Risk Assessment:** **LOW** - Current workaround is acceptable for production

---

## ✅ BUG #20: Modbus RTU Timeout Handling

### Status
✅ **FIXED** - Resolved by recent enhancement

### Severity
**N/A** - Already fixed

### Description
Modbus RTU timeout handling was improved as part of the "Flexible Device Control" enhancement (completed November 21, 2025).

### What Was Fixed

**Before (v2.1.1 and earlier):**
- Basic timeout counter
- Device disabled after max retries
- **No auto-recovery** - manual restart required
- **No metrics tracking** - no visibility into timeout patterns

**After (v2.2.0+ - Our Enhancement):**
- ✅ **DeviceReadTimeout** struct tracks consecutive timeouts (ModbusRtuService.h:83-91)
- ✅ **Exponential backoff** with jitter (ModbusRtuService.cpp:1093-1105)
- ✅ **handleReadTimeout()** distinguishes timeout vs retry failures (ModbusRtuService.cpp:1135-1154)
- ✅ **Auto-recovery task** re-enables devices every 5 minutes (ModbusRtuService.cpp:1467-1499)
- ✅ **Health metrics** track timeout history (ModbusRtuService.h:85-126)
- ✅ **DisableReason** tracking: `AUTO_TIMEOUT` vs `MANUAL` (ModbusRtuService.h:70-75)

**Files Modified:**
- `Main/ModbusRtuService.h` (+72 lines)
- `Main/ModbusRtuService.cpp` (+183 lines)
- Same improvements applied to ModbusTcpService

**Commits:**
- `d7c647a` - Core functionality (RTU + TCP)
- `2c50266` - BLE commands + documentation

**Verification:**
```cpp
// Auto-recovery runs every 5 minutes
const unsigned long RECOVERY_INTERVAL_MS = 300000; // Line 1470

// Only recovers AUTO-disabled devices
if (!state.isEnabled &&
    (state.disableReason == DeviceFailureState::AUTO_RETRY ||
     state.disableReason == DeviceFailureState::AUTO_TIMEOUT))
{
  enableDevice(state.deviceId.c_str(), false); // Line 1492
}
```

### Status: **RESOLVED** ✅

---

## 🟢 BUG #22: LED Indicators Not Syncing

### Status
⏳ **PENDING ANALYSIS** - Requires code review

### Severity
**LOW** - Low priority, low effort

### Description
LED indicators reported to not sync properly with system state.

### Investigation Required

**Files to Check:**
- `Main/LEDManager.h/.cpp` - LED control logic
- `Main/Main.ino` - LED update calls in loop()

**Common Causes:**
1. LED update not called frequently enough
2. Race condition in LED state updates
3. Missing mutex protection for shared LED state
4. FreeRTOS task priority issues

**Recommended Next Steps:**
1. Check if `LEDManager::update()` is called in `loop()`
2. Verify LED update frequency (should be ~100ms for smooth blinking)
3. Check for mutex protection on LED state variables
4. Test with different task priorities

**Estimated Effort:** ~30 minutes (investigation) + ~1 hour (fix if confirmed)

---

## 🟢 BUG #23: RTC Time Drift After 24 Hours

### Status
⏳ **PENDING ANALYSIS** - Requires testing

### Severity
**LOW** - Low priority, low effort

### Description
RTC (DS3231) time reported to drift after 24 hours.

### Investigation Required

**Files to Check:**
- `Main/RTCManager.h/.cpp` - RTC initialization and sync logic
- Check if NTP sync is enabled and working

**Common Causes:**
1. **DS3231 aging offset** not calibrated
2. **NTP sync disabled** or failing
3. **Temperature compensation** not working
4. **Battery backup** issue (clock stops during power loss)

**DS3231 Specs:**
- Accuracy: ±2ppm (0-40°C) = ±5.2 seconds/month
- With aging offset: ±1ppm possible
- 24-hour drift should be <0.2 seconds if properly calibrated

**Recommended Next Steps:**
1. Check if NTP sync is enabled: `RTCManager::syncWithNTP()`
2. Verify NTP sync interval (should be every 1-6 hours)
3. Check DS3231 aging offset register
4. Monitor actual drift over 24 hours with serial logs

**Estimated Effort:** ~2 hours (investigation + calibration)

---

## 🟡 BUG #24: Config File Validation Missing

### Status
⚠️ **PARTIAL** - Some validation exists, needs improvement

### Severity
**MEDIUM** - Medium priority, low effort

### Description
Configuration file validation is incomplete. System may accept invalid configs.

### Current Status

**Existing Validation:**
- ✅ JSON parsing validation (ArduinoJson returns error codes)
- ✅ Atomic writes with WAL (prevents file corruption)
- ⚠️ **Schema validation missing** - no field type/range checks

**Files:**
- `Main/ConfigManager.h/.cpp` - Config loading/saving
- `Main/AtomicFileOps.h/.cpp` - WAL implementation

### Recommended Improvements

**Add Schema Validation:**

```cpp
bool ConfigManager::validateDeviceConfig(const JsonObject &device) {
  // Required fields
  if (!device.containsKey("device_id") ||
      !device.containsKey("protocol") ||
      !device.containsKey("slave_id")) {
    return false;
  }

  // Protocol validation
  String protocol = device["protocol"] | "";
  if (protocol != "RTU" && protocol != "TCP") {
    return false;
  }

  // Slave ID range (1-247 for Modbus)
  int slaveId = device["slave_id"] | 0;
  if (slaveId < 1 || slaveId > 247) {
    return false;
  }

  // RTU-specific validation
  if (protocol == "RTU") {
    int serialPort = device["serial_port"] | 0;
    if (serialPort != 1 && serialPort != 2) {
      return false;
    }

    int baudRate = device["baud_rate"] | 0;
    if (baudRate != 1200 && baudRate != 2400 &&
        baudRate != 4800 && baudRate != 9600 &&
        baudRate != 19200 && baudRate != 38400 &&
        baudRate != 57600 && baudRate != 115200) {
      return false;
    }
  }

  // Registers validation
  if (device.containsKey("registers") && device["registers"].is<JsonArray>()) {
    JsonArray registers = device["registers"];
    for (JsonObject reg : registers) {
      if (!validateRegisterConfig(reg)) {
        return false;
      }
    }
  }

  return true;
}

bool ConfigManager::validateRegisterConfig(const JsonObject &reg) {
  // Required fields
  if (!reg.containsKey("address") ||
      !reg.containsKey("function_code") ||
      !reg.containsKey("data_type")) {
    return false;
  }

  // Function code validation (1-4 for Modbus)
  int funcCode = reg["function_code"] | 0;
  if (funcCode < 1 || funcCode > 4) {
    return false;
  }

  // Address range (0-65535)
  int address = reg["address"] | -1;
  if (address < 0 || address > 65535) {
    return false;
  }

  return true;
}
```

**Integrate Validation:**

```cpp
bool ConfigManager::createDevice(const JsonObject &device) {
  // NEW: Validate before saving
  if (!validateDeviceConfig(device)) {
    Serial.println("[ConfigManager] ERROR: Invalid device configuration");
    return false;
  }

  // Existing save logic...
  return saveDevicesConfig(devices);
}
```

**Estimated Effort:** ~2 hours (implementation + testing)

---

## 🟢 BUG #25: Button Debouncing Issues

### Status
⏳ **PENDING ANALYSIS** - Requires testing

### Severity
**LOW** - Low priority, low effort

### Description
Button debouncing reported to have issues (double-triggers or missed presses).

### Investigation Required

**Files to Check:**
- `Main/ButtonManager.h/.cpp` - Button handling logic
- Verify if using `OneButton` library (which has built-in debouncing)

**Common Causes:**
1. **Debounce time too short** (should be 50-100ms)
2. **Interrupt-based** without proper debouncing
3. **Polling frequency** too low (should be ~10-50ms)
4. **Hardware noise** - missing pull-up/pull-down resistor

**OneButton Library Defaults:**
- Debounce time: 50ms (usually sufficient)
- Long press: 1000ms
- Click detection: works well

**Recommended Next Steps:**
1. Check if `OneButton` library is used
2. Verify debounce time setting
3. Test with oscilloscope to see actual signal
4. Check if hardware pull-up resistor is present

**Estimated Effort:** ~30 minutes (investigation) + ~30 minutes (fix if needed)

---

## 🟢 BUG #26: Error Codes Not Consistent

### Status
⚠️ **PARTIAL** - System exists but not fully utilized

### Severity
**LOW** - Low priority, low effort

### Description
Error codes not consistently used across all modules.

### Current Status

**Existing System:**
- ✅ `UnifiedErrorCodes.h` defines error ranges (0-599)
- ✅ `ErrorHandler` class exists for centralized reporting
- ⚠️ **Not all modules use it** - some still use raw Serial.println()

**Error Code Ranges:**
```cpp
// Network errors (0-99)
ERR_NET_WIFI_DISCONNECTED = 1
ERR_NET_ETH_CABLE_UNPLUGGED = 2

// MQTT errors (100-199)
ERR_MQTT_CONNECTION_FAILED = 101
ERR_MQTT_PUBLISH_FAILED = 102

// BLE errors (200-299)
ERR_BLE_INIT_FAILED = 201
ERR_BLE_MTU_NEGOTIATION_TIMEOUT = 202

// Modbus errors (300-399)
ERR_MODBUS_DEVICE_TIMEOUT = 301
ERR_MODBUS_INVALID_RESPONSE = 302

// Memory errors (400-499)
ERR_MEMORY_ALLOCATION_FAILED = 401
ERR_MEMORY_PSRAM_INIT_FAILED = 402

// Config errors (500-599)
ERR_CONFIG_FILE_NOT_FOUND = 501
ERR_CONFIG_JSON_PARSE_FAILED = 502
```

### Recommended Fix

**Audit and Replace:**

1. Search for direct `Serial.println` error messages:
```bash
$ grep -rn "Serial.println.*ERROR\|Serial.printf.*ERROR" Main/ | wc -l
# Count how many need replacement
```

2. Replace with `ErrorHandler`:
```cpp
// OLD:
Serial.println("[BLE] ERROR: MTU negotiation timeout");

// NEW:
ErrorHandler::getInstance()->reportError(
  ERR_BLE_MTU_NEGOTIATION_TIMEOUT,
  "BLE",
  mtuNegotiationAttempts
);
```

3. Add missing error codes to `UnifiedErrorCodes.h`:
```cpp
// Add any missing error codes for new features
enum UnifiedErrorCode {
  // ... existing codes ...

  // Device Control errors (600-699)
  ERR_DEVICE_CONTROL_INVALID_COMMAND = 601,
  ERR_DEVICE_CONTROL_DEVICE_NOT_FOUND = 602,
  ERR_DEVICE_CONTROL_PERMISSION_DENIED = 603,
};
```

**Estimated Effort:** ~3 hours (audit + replace ~50-100 calls)

---

## 📊 Priority Matrix

### Immediate Action Required (Next 1-2 Days)
1. **BUG #17** - Network Failover (High priority, critical impact)

### Short-term Action (Next 1-2 Weeks)
2. **BUG #18** - BLE MTU Timeout (Easy fix - 1 line)
3. **BUG #24** - Config Validation (Medium priority, prevents invalid configs)

### Long-term Improvements (Next 1-2 Months)
4. **BUG #26** - Error Code Consistency (Low priority, improves debugging)
5. **BUG #22** - LED Syncing (Low priority, cosmetic)
6. **BUG #23** - RTC Drift (Low priority, requires calibration)
7. **BUG #25** - Button Debouncing (Low priority, may already work)

### Already Fixed ✅
- **BUG #20** - Modbus RTU Timeout (Fixed in v2.2.0 enhancement)

---

## 🔧 Quick Wins (< 30 minutes each)

These bugs can be fixed quickly for immediate improvement:

### 1. BUG #18 - Add MTU Timeout Check (15 minutes)
**File:** `Main/BLEManager.cpp`
**Line:** Find `metricsMonitorLoop()`, add `checkMTUNegotiationTimeout();`

### 2. BUG #24 - Add Basic Validation (30 minutes)
**File:** `Main/ConfigManager.cpp`
**Action:** Add `validateDeviceConfig()` to `createDevice()` and `updateDevice()`

---

## 📝 Testing Recommendations

### For BUG #17 (Network Failover):
```cpp
// Test scenario
1. Connect to WiFi
2. Disconnect WiFi physically
3. Wait 5 seconds
4. Verify automatic switch to Ethernet
5. Serial log should show: "[NetworkMgr] WIFI unhealthy, attempting failover..."
```

### For BUG #18 (BLE MTU):
```cpp
// Test scenario
1. Connect BLE client
2. Force MTU negotiation delay (iOS clients may be slow)
3. After 2 seconds, check serial log
4. Should show: "[BLE MTU] WARNING: Timeout detected after 2000 ms"
```

### For BUG #20 (Modbus Timeout):
```cpp
// Already tested - works
1. Disconnect Modbus slave
2. Wait for device to auto-disable (max retries)
3. Wait 5 minutes
4. Device should auto-enable
5. Serial log shows: "[RTU AutoRecovery] Device D7A3F2 re-enabled"
```

---

## 🎯 Summary

**Critical Issues:** 1 (BUG #17 - Network Failover)
**Fixed Issues:** 1 (BUG #20 - Modbus Timeout)
**Minor Issues:** 6 (BUG #18, #22-26)

**Recommended Next Steps:**
1. **Immediate:** Fix BUG #17 (Network Failover Monitoring)
2. **Quick Win:** Fix BUG #18 (MTU Timeout Check - 1 line)
3. **Short-term:** Add Config Validation (BUG #24)
4. **Long-term:** Error code consistency audit (BUG #26)

**Total Estimated Effort:**
- Critical fixes: ~2.5 hours
- All fixes: ~10-12 hours total

---

**Report Generated:** November 21, 2025
**Analyst:** Claude AI
**Firmware Version:** v2.2.0+
**Repository:** GatewaySuriotaPOC

**Made with ❤️ by SURIOTA R&D Team**
