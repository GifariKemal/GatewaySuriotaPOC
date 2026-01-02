# 🐛 Bug Status Report - Updated December 10, 2025

**SRT-MGATE-1210 Firmware - Bug Analysis** **Version:** v2.5.34 **Last
Updated:** 2025-12-10 | **Original Report:** 2025-11-21

---

## 📋 Executive Summary

Analysis of 8 reported bugs with priority classification, root cause analysis,
and fix status.

| Bug #   | Priority  | Component         | Status            | Severity | Fix Version   |
| ------- | --------- | ----------------- | ----------------- | -------- | ------------- |
| **#17** | 🟢 Fixed  | NetworkManager    | ✅ **FIXED**      | N/A      | v2.5.33       |
| **#18** | 🟡 Medium | BLEManager        | ⚠️ **ACCEPTABLE** | Low      | Workaround OK |
| **#20** | 🟢 Fixed  | ModbusRtuService  | ✅ **FIXED**      | N/A      | v2.2.0        |
| **#22** | 🟢 Low    | LEDManager        | ⏳ **PENDING**    | Low      | -             |
| **#23** | 🟢 Low    | RTCManager        | ⏳ **PENDING**    | Low      | -             |
| **#24** | 🟡 Medium | ConfigManager     | ⚠️ **PARTIAL**    | Medium   | -             |
| **#25** | 🟢 Low    | ButtonManager     | ⏳ **PENDING**    | Low      | -             |
| **#26** | 🟢 Low    | UnifiedErrorCodes | ⚠️ **PARTIAL**    | Low      | -             |

**Update December 10, 2025:**

- ✅ **BUG #17 NOW FIXED** - Network failover task implemented in v2.5.33
- ✅ **BUG #20 CONFIRMED FIXED** - Auto-recovery working since v2.2.0
- ⚠️ **BUG #18 ACCEPTABLE** - Current workaround sufficient for production

---

## ✅ BUG #17: Network Failover - **FIXED in v2.5.33**

### Status

✅ **FIXED** - Implemented in v2.5.33

### Original Issue (November 2025)

Network failover mechanism existed but was not actively monitored. No FreeRTOS
task to detect connection failures.

### Resolution (v2.5.33)

**Implementation Details:**

1. **Failover Task Created** (`NetworkManager.cpp:196-212`):

```cpp
void NetworkMgr::startFailoverTask() {
    if (failoverTaskHandle == nullptr) {
        xTaskCreatePinnedToCore(
            failoverTask,
            "NET_FAILOVER_TASK",
            4096,
            this,
            1,
            &failoverTaskHandle,
            0  // Core 0 for load balancing
        );
        LOG_NET_INFO("[NETWORK] Failover task started");
    }
}
```

2. **Auto-Reconnect Logic** (`NetworkManager.cpp:230-247`):

```cpp
// v2.5.33: Try to reconnect uninitialized networks periodically
if (now - lastReconnectCheck >= reconnectInterval) {
    lastReconnectCheck = now;

    // Try to reconnect WiFi if down at startup
    if (wifiManager && wifiManager->hasStoredConfig() && !wifiManager->isInitialized()) {
        wifiManager->tryReconnect();
    }

    // Try to reconnect Ethernet if down at startup
    if (ethernetManager && ethernetManager->hasStoredConfig() && !ethernetManager->isInitialized()) {
        ethernetManager->tryReconnect();
    }
}
```

3. **Automatic Failover Switching** (`NetworkManager.cpp:287-333`):
   - Monitors primary and secondary network availability
   - Automatically switches when primary fails
   - Switches back when primary recovers

**Verification:**

- ✅ `NET_FAILOVER_TASK` runs on Core 0
- ✅ Checks network health every 5 seconds (configurable)
- ✅ Thread-safe mode switching with mutex protection
- ✅ Auto-reconnect for networks down at startup

### Status: **RESOLVED** ✅

---

## ⚠️ BUG #18: BLE MTU Negotiation Timeout - **ACCEPTABLE**

### Status

⚠️ **ACCEPTABLE** - Current workaround sufficient for production

### Original Issue

MTU negotiation timeout handling exists but `checkMTUNegotiationTimeout()` is
not called periodically.

### Current Workaround (Acceptable)

The current 200ms delay + `logMTUNegotiation()` provides a practical workaround:

- Most clients negotiate within 100-200ms
- After 200ms, system logs actual MTU
- If negotiation failed, falls back to 23 bytes (works)
- Large responses handled via fragmentation (already implemented)

### Risk Assessment

**LOW** - Current workaround is acceptable for production. MTU negotiation
hanging is extremely rare.

### Recommendation

No immediate action required. Can be improved as a future enhancement if needed.

### Status: **ACCEPTABLE WORKAROUND** ⚠️

---

## ✅ BUG #20: Modbus RTU Timeout Handling - **FIXED in v2.2.0**

### Status

✅ **FIXED** - Resolved in v2.2.0

### What Was Fixed

**Before (v2.1.1 and earlier):**

- Basic timeout counter
- Device disabled after max retries
- No auto-recovery - manual restart required
- No metrics tracking

**After (v2.2.0+):**

- ✅ **DeviceReadTimeout** struct tracks consecutive timeouts
- ✅ **Exponential backoff** with jitter
- ✅ **handleReadTimeout()** distinguishes timeout vs retry failures
- ✅ **Auto-recovery task** re-enables devices every 5 minutes
- ✅ **Health metrics** track timeout history
- ✅ **DisableReason** tracking: `AUTO_TIMEOUT` vs `MANUAL`

### Verification

```cpp
// Auto-recovery runs every 5 minutes
const unsigned long RECOVERY_INTERVAL_MS = 300000;

// Only recovers AUTO-disabled devices
if (!state.isEnabled &&
    (state.disableReason == DeviceFailureState::AUTO_RETRY ||
     state.disableReason == DeviceFailureState::AUTO_TIMEOUT))
{
  enableDevice(state.deviceId.c_str(), false);
}
```

### Status: **RESOLVED** ✅

---

## 🟢 BUG #22: LED Indicators Not Syncing

### Status

⏳ **PENDING ANALYSIS** - Low priority

### Severity

**LOW** - Cosmetic issue

### Investigation Required

- Check if `LEDManager::update()` is called frequently
- Verify LED update frequency (~100ms for smooth blinking)
- Check for mutex protection on LED state variables

### Status: **PENDING** ⏳

---

## 🟢 BUG #23: RTC Time Drift After 24 Hours

### Status

⏳ **PENDING ANALYSIS** - Low priority

### Severity

**LOW** - Minor timing issue

### Investigation Required

- Check if NTP sync is enabled and working
- Verify NTP sync interval (should be every 1-6 hours)
- Check DS3231 aging offset register

### Status: **PENDING** ⏳

---

## 🟡 BUG #24: Config File Validation Missing

### Status

⚠️ **PARTIAL** - Some validation exists

### Severity

**MEDIUM** - May accept invalid configs

### Current Status

- ✅ JSON parsing validation (ArduinoJson returns error codes)
- ✅ Atomic writes with WAL (prevents file corruption)
- ⚠️ Schema validation missing - no field type/range checks

### Recommendation

Add schema validation for device and register configurations:

- Protocol validation (RTU/TCP)
- Slave ID range (1-247)
- Baud rate validation
- Function code validation (1-4)
- Address range validation (0-65535)

### Status: **PARTIAL** ⚠️

---

## 🟢 BUG #25: Button Debouncing Issues

### Status

⏳ **PENDING ANALYSIS** - Low priority

### Severity

**LOW** - Minor UX issue

### Notes

Currently using OneButton library which has built-in debouncing (50ms default).

### Status: **PENDING** ⏳

---

## 🟢 BUG #26: Error Codes Not Consistent

### Status

⚠️ **PARTIAL** - System exists but not fully utilized

### Severity

**LOW** - Debugging inconvenience

### Current Status

- ✅ `UnifiedErrorCodes.h` defines error ranges (0-599)
- ✅ `ErrorHandler` class exists for centralized reporting
- ⚠️ Not all modules use it - some still use raw Serial.println()

### Status: **PARTIAL** ⚠️

---

## 📊 Updated Priority Matrix

### ✅ Fixed (No Action Required)

1. **BUG #17** - Network Failover ✅ Fixed in v2.5.33
2. **BUG #20** - Modbus RTU Timeout ✅ Fixed in v2.2.0

### ⚠️ Acceptable Workaround

3. **BUG #18** - BLE MTU Timeout (current workaround sufficient)

### 🔧 Future Improvements (Low Priority)

4. **BUG #24** - Config Validation (medium priority)
5. **BUG #26** - Error Code Consistency (low priority)
6. **BUG #22** - LED Syncing (low priority, cosmetic)
7. **BUG #23** - RTC Drift (low priority)
8. **BUG #25** - Button Debouncing (low priority)

---

## 🎯 Summary

**Fixed Issues:** 2 (BUG #17, #20) **Acceptable Workaround:** 1 (BUG #18)
**Pending/Partial:** 5 (BUG #22-26)

**Critical Issues:** **NONE** - All critical bugs have been resolved.

**Firmware Status:** Production-ready (v2.5.34)

---

## 📝 Revision History

| Date       | Version | Changes                                              |
| ---------- | ------- | ---------------------------------------------------- |
| 2025-12-10 | v2.5.34 | Updated BUG #17 to FIXED (failover task implemented) |
| 2025-11-21 | v2.2.0  | Initial bug report, BUG #20 fixed                    |

---

**Report Updated:** December 10, 2025 **Firmware Version:** v2.5.34
**Repository:** GatewaySuriotaPOC

**Made with ❤️ by SURIOTA R&D Team**
