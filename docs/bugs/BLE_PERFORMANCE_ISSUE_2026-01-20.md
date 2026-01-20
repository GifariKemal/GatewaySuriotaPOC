# Bug Report: BLE Read Device Performance Issue

**Report Date:** 2026-01-20  
**Reporter:** Mobile Team  
**Severity:** HIGH  
**Component:** Firmware - BLE Communication, RTU Auto-Recovery, MQTT  
**Affected Version:** Current Production Firmware

---

## 📋 Executive Summary

BLE read device command experiencing severe performance degradation (~28+ seconds response time) and occasional device disconnection when reading device with 45 registers. Root cause analysis reveals **firmware-side resource contention** between RTU auto-recovery, MQTT reconnection, and BLE command processing.

---

## 🔴 Problem Statement

### Observed Behavior

When mobile app sends BLE command to read device data (45 registers):

- ❌ Response time: **~28+ seconds** (expected: 3-5 seconds)
- ❌ Occasional BLE disconnection
- ❌ Device becomes unresponsive during operation
- ❌ MQTT connection drops repeatedly

### Expected Behavior

- ✅ Response time: **3-5 seconds** for 45 registers
- ✅ Stable BLE connection throughout operation
- ✅ Device remains responsive
- ✅ MQTT connection remains stable

---

## 🔍 Root Cause Analysis

### Timeline of Events (from Firmware Logs)

```
Time        | Event                                    | Duration | Impact
------------|------------------------------------------|----------|------------------
20:59:12    | BLE command received                     | 0s       | ✓ Command received
20:59:12    | MQTT reconnect starts                    | 6s       | ⚠️ Blocks resources
20:59:12    | Device read (9783 bytes)                 | 0s       | ✓ Data ready
20:59:12    | LOW DRAM detected (13.5KB)               | -        | ⚠️ Forces SLOW mode
20:59:12    | BLE transmission (SMALL chunks, 35ms)    | ~3.4s    | ⚠️ Slow transmission
20:59:13    | MQTT connection lost                     | +1s      | ❌ Resource waste
20:59:14    | MQTT reconnect                           | +1s      | ❌ Resource waste
20:59:17    | MQTT connection lost again               | +3s      | ❌ Resource waste
20:59:20    | MQTT reconnect (2.3s)                    | +3s      | ❌ Resource waste
20:59:26    | RTU auto-recovery starts (3 devices)     | +11s     | ❌ CPU blocking
            | - Def004: 5 retries × ~2s = 10s          |          |
            | - D65f89: 5 retries × ~2s = 10s          |          |
            | - Dc1914: 5 retries × ~2s = 10s          |          |
------------|------------------------------------------|----------|------------------
TOTAL       |                                          | ~28s+    | ❌ UNACCEPTABLE
```

### Primary Issues Identified

#### 1. **RTU Auto-Recovery Blocking CPU** (Critical)

**Evidence from logs:**

```
[21:14:47][ERROR][RTU] Device Def004: All 5 register reads failed
[21:14:47][INFO][RTU] Exponential backoff: retry 5 to 1600ms base + 642ms jitter = 2242ms total
[21:14:47][INFO][RTU] Device Def004 read failed. Retry 5/5 in 2242ms

[21:14:47][ERROR][RTU] Device D65f89: All 4 register reads failed
[21:14:47][INFO][RTU] Exponential backoff: retry 5 to 1600ms base + 70ms jitter = 1670ms total

[21:14:48][ERROR][RTU] Device Dc1914: All 1 register reads failed
[21:14:48][INFO][RTU] Exponential backoff: retry 4 to 800ms base + 291ms jitter = 1091ms total
```

**Impact:**

- 3 RTU devices failing continuously
- Each device: 5 retries with exponential backoff (100ms → 200ms → 400ms → 800ms → 1600ms)
- Total retry time per device: **~4-5 seconds**
- Total for 3 devices: **~12-15 seconds**
- Retry cycle repeats every ~20 seconds
- **CPU blocked during retry operations**

**Suppressed messages:**

```
(suppressed 309 similar messages in 30s: "RTU Device D65f89 disabled")
```

This indicates the retry loop is running **continuously** in the background.

#### 2. **MQTT Connection Instability** (High)

**Evidence from logs:**

```
[20:59:12][INFO][MQTT] Connection attempt took 6069 ms (slow)
[20:59:12][INFO][MQTT] Connected
[20:59:13][INFO][MQTT] Connection lost (1 second later!)
[20:59:14][INFO][MQTT] Connected
[20:59:17][INFO][MQTT] Connection lost (3 seconds later!)
[20:59:20][INFO][MQTT] Connection attempt took 2368 ms (slow)
[20:59:26][INFO][MQTT] Connection lost
[20:59:36][INFO][MQTT] Connection attempt took 9785 ms (VERY slow!)
```

**Impact:**

- MQTT reconnect occurs **5 times** in 24 seconds
- Each reconnect: 2-10 seconds
- Total MQTT overhead: **~18-20 seconds**
- Network: WiFi (RSSI: -51 to -63 dBm - acceptable signal)
- Broker: `broker.hivemq.com:1883` (public broker - may be unstable)

#### 3. **Low DRAM Memory** (Medium)

**Evidence from logs:**

```
[20:59:12][INFO][CRUD] Large device detected (45 regs). Free DRAM: 13284 bytes
[20:59:12][INFO][BLE] WARNING: Low DRAM (13504 bytes). Proceeding with caution.
[20:59:12][INFO][BLE] Large payload (9783 bytes) + LOW DRAM (13504 bytes) = using SMALL chunks with SLOW delay (size:100, delay:35ms)
```

**Impact:**

- Free DRAM: **13.5KB** (critically low)
- Forces BLE to use SMALL chunks (100 bytes) instead of optimal size (512 bytes)
- Forces SLOW delay (35ms) instead of fast delay (10ms)
- Payload: 9783 bytes ÷ 100 bytes/chunk = **98 chunks**
- Transmission time: 98 × 35ms = **~3.4 seconds** (vs ~0.5s with optimal settings)

#### 4. **No BLE Priority Management** (Critical)

**Observation:**

- RTU auto-recovery runs **during** BLE command processing
- MQTT reconnect runs **during** BLE command processing
- No evidence of BLE command being prioritized over background tasks

---

## 🎯 Recommended Solutions

### Solution 1: Implement BLE Priority Management (CRITICAL - Must Fix)

**Priority Order:**

1. **BLE Commands** (Highest Priority)
2. **TCP/RTU Device Polling** (Medium Priority)
3. **MQTT Operations** (Low Priority)

**Implementation:**

```cpp
// Pseudo-code for firmware
bool ble_command_active = false;

void processBLECommand() {
  ble_command_active = true;

  // Pause all background tasks
  pause_rtu_auto_recovery();
  pause_mqtt_keepalive();

  // Process BLE command
  handle_ble_command();

  // Resume background tasks
  resume_rtu_auto_recovery();
  resume_mqtt_keepalive();

  ble_command_active = false;
}

void processRTUAutoRecovery() {
  // Skip if BLE is active
  if (ble_command_active) {
    return;
  }

  // Process RTU recovery
  // ...
}

void processMQTT() {
  // Skip if BLE is active
  if (ble_command_active) {
    return;
  }

  // Process MQTT
  // ...
}
```

**Expected Impact:**

- BLE response time: **28s → 3-5s** (80% improvement)
- No more BLE disconnections during command processing

---

### Solution 2: Disable/Fix RTU Devices (HIGH Priority)

**Current Issue:**

- 3 RTU devices (Def004, D65f89, Dc1914) are **not connected** but firmware keeps retrying
- Each retry cycle: **~12-15 seconds**

**Recommended Actions:**

**Option A: Disable Auto-Recovery for Disconnected Devices**

```cpp
// Increase auto-recovery interval from 20s to 5 minutes
#define RTU_AUTO_RECOVERY_INTERVAL_MS (5 * 60 * 1000)  // 5 minutes

// Or disable auto-recovery when BLE is active
if (!ble_command_active && should_auto_recover()) {
  attempt_rtu_recovery();
}
```

**Option B: Remove Unconfigured Devices**

- Delete device configurations for Def004, D65f89, Dc1914 if not in use
- Or fix RTU connection (check wiring, baudrate, slave ID)

**Expected Impact:**

- CPU usage: **-30%**
- BLE response time: **-10-15 seconds**

---

### Solution 3: Fix MQTT Connection Stability (MEDIUM Priority)

**Current Issue:**

- MQTT reconnect every **2-7 seconds**
- Using public broker: `broker.hivemq.com` (may be unstable)

**Recommended Actions:**

**Option A: Use Local/Private MQTT Broker**

```cpp
// Change from public to local broker
#define MQTT_BROKER "192.168.1.100"  // Local broker
// Instead of: broker.hivemq.com
```

**Option B: Increase MQTT Keepalive**

```cpp
// Increase keepalive from default (15s) to 60-120s
#define MQTT_KEEPALIVE_SECONDS 60

// Add connection stability check
if (mqtt_connection_stable_for(30)) {
  // Connection is stable, continue
} else {
  // Reconnect with backoff
}
```

**Option C: Pause MQTT During BLE Operations**

```cpp
void processBLECommand() {
  mqtt_pause_keepalive();
  handle_ble_command();
  mqtt_resume_keepalive();
}
```

**Expected Impact:**

- MQTT overhead: **-18-20 seconds**
- Network stability: **+50%**

---

### Solution 4: Optimize Memory Usage (LOW Priority)

**Current Issue:**

- Free DRAM: **13.5KB** (critically low)
- Forces SLOW BLE transmission mode

**Recommended Actions:**

**Option A: Implement Pagination for Large Devices**

```cpp
// Split large device reads into pages
if (register_count > 20) {
  // Return paginated response
  send_paginated_response(page_size: 20);
} else {
  // Return full response
  send_full_response();
}
```

**Option B: Optimize Memory Allocation**

```cpp
// Free unused memory before BLE operations
void prepare_for_ble_command() {
  free_mqtt_buffers();
  free_rtu_buffers();
  compact_heap();
}
```

**Expected Impact:**

- Free DRAM: **13.5KB → 30KB+**
- BLE chunk size: **100 bytes → 256-512 bytes**
- BLE transmission: **~3.4s → ~0.5-1s**

---

## 📊 Performance Comparison

### Current Performance (Before Fix)

| Metric               | Value        | Status      |
| -------------------- | ------------ | ----------- |
| BLE Response Time    | ~28+ seconds | ❌ CRITICAL |
| Free DRAM            | 13.5KB       | ⚠️ LOW      |
| BLE Chunk Size       | 100 bytes    | ⚠️ SLOW     |
| BLE Delay            | 35ms         | ⚠️ SLOW     |
| MQTT Reconnect       | 5x in 24s    | ❌ UNSTABLE |
| RTU Retry Overhead   | ~12-15s      | ❌ BLOCKING |
| Device Disconnection | Occasional   | ❌ CRITICAL |

### Expected Performance (After Fix)

| Metric               | Value           | Status         |
| -------------------- | --------------- | -------------- |
| BLE Response Time    | **3-5 seconds** | ✅ GOOD        |
| Free DRAM            | 30KB+           | ✅ GOOD        |
| BLE Chunk Size       | 256-512 bytes   | ✅ OPTIMAL     |
| BLE Delay            | 10-20ms         | ✅ FAST        |
| MQTT Reconnect       | Stable          | ✅ STABLE      |
| RTU Retry Overhead   | 0s (paused)     | ✅ NO BLOCKING |
| Device Disconnection | None            | ✅ STABLE      |

**Overall Improvement: ~80-85% faster**

---

## 🧪 Test Scenarios

### Test Case 1: BLE Read Device (45 Registers)

**Steps:**

1. Connect to gateway via BLE
2. Send read device command for device with 45 registers
3. Measure response time

**Expected Result:**

- Response time: **< 5 seconds**
- No BLE disconnection
- No MQTT reconnect during operation

### Test Case 2: BLE Read Device with RTU Devices Active

**Steps:**

1. Configure 3 RTU devices (all connected)
2. Connect to gateway via BLE
3. Send read device command
4. Verify RTU polling is paused during BLE operation

**Expected Result:**

- Response time: **< 5 seconds**
- RTU polling resumes after BLE command complete
- No retry loop during BLE operation

### Test Case 3: BLE Read Device with MQTT Active

**Steps:**

1. Connect gateway to MQTT broker
2. Connect to gateway via BLE
3. Send read device command
4. Verify MQTT keepalive is paused during BLE operation

**Expected Result:**

- Response time: **< 5 seconds**
- No MQTT reconnect during BLE operation
- MQTT connection remains stable

---

## 📝 Implementation Priority

| Priority | Solution                             | Estimated Effort | Impact     |
| -------- | ------------------------------------ | ---------------- | ---------- |
| **P0**   | BLE Priority Management              | 2-3 days         | ⭐⭐⭐⭐⭐ |
| **P1**   | Disable RTU Auto-Recovery During BLE | 1 day            | ⭐⭐⭐⭐   |
| **P1**   | Pause MQTT During BLE                | 1 day            | ⭐⭐⭐⭐   |
| **P2**   | Fix MQTT Connection Stability        | 2-3 days         | ⭐⭐⭐     |
| **P3**   | Optimize Memory Usage                | 3-5 days         | ⭐⭐       |

**Total Estimated Effort:** 1-2 weeks for P0-P1 fixes

---

## 📎 Appendix: Full Firmware Logs

### Log Sample 1: RTU Auto-Recovery Blocking

```
[2026-01-20 21:14:47][ERROR][RTU] Device Def004: All 5 register reads failed
[2026-01-20 21:14:47][INFO][RTU] [RTU] Exponential backoff: retry 5 to 1600ms base + 642ms jitter = 2242ms total
[2026-01-20 21:14:47][INFO][RTU] [RTU] Device Def004 read failed. Retry 5/5 in 2242ms
D65f89: Temperature_INT16 = ERROR
D65f89: Humidity_INT16 = ERROR
D65f89: Temperature_FLOAT = ERROR
D65f89: Config_Baudrate = ERROR
[2026-01-20 21:14:47][ERROR][RTU] Device D65f89: All 4 register reads failed
[2026-01-20 21:14:47][INFO][RTU] [RTU] Exponential backoff: retry 5 to 1600ms base + 70ms jitter = 1670ms total
[2026-01-20 21:14:47][INFO][RTU] [RTU] Device D65f89 read failed. Retry 5/5 in 1670ms
Dc1914: Temperature_1 = ERROR
[2026-01-20 21:14:48][ERROR][RTU] Device Dc1914: All 1 register reads failed
[2026-01-20 21:14:48][INFO][RTU] [RTU] Exponential backoff: retry 4 to 800ms base + 291ms jitter = 1091ms total
```

### Log Sample 2: MQTT Reconnection Loop

```
[2026-01-20 21:14:50][INFO][MQTT] [MQTT] Connection attempt took 5180 ms (slow)
[2026-01-20 21:14:50][INFO][MQTT] [MQTT] Connected | Broker: broker.hivemq.com:1883 | Network: WIFI (192.168.100.161)
[2026-01-20 21:14:51][INFO][MQTT] [MQTT] Connection lost, attempting reconnect...
[2026-01-20 21:15:07][INFO][MQTT] [MQTT] Connection attempt took 2860 ms (slow)
[2026-01-20 21:15:07][INFO][MQTT] [MQTT] Connected | Broker: broker.hivemq.com:1883 | Network: WIFI (192.168.100.161)
[2026-01-20 21:15:13][INFO][MQTT] [MQTT] Connection lost, attempting reconnect...
```

### Log Sample 3: Low DRAM Warning

```
[2026-01-20 20:59:12][INFO][CRUD] [CRUD] Large device detected (45 regs). Free DRAM: 13284 bytes
[2026-01-20 20:59:12][INFO][BLE] [BLE] WARNING: Low DRAM (13504 bytes). Proceeding with caution.
[2026-01-20 20:59:12][INFO][BLE] [BLE] Large payload (9783 bytes) + LOW DRAM (13504 bytes) = using SMALL chunks with SLOW delay (size:100, delay:35ms)
```

---

## 🔗 Related Issues

- None (new issue)

## 📧 Contact

**Reporter:** Mobile Development Team  
**Date:** 2026-01-20  
**Firmware Version:** Current Production

---

**End of Report**
