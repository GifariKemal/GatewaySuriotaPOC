# Bug Report: Firmware Task Priority & Resource Contention

**Report Date:** 2026-01-20  
**Reporter:** Firmware Analysis Team  
**Severity:** CRITICAL  
**Component:** Firmware - Task Scheduling, Resource Management  
**Affected Version:** Current Production Firmware (v1.0.6+)  
**Related Bug:** BLE_PERFORMANCE_ISSUE_2026-01-20.md

---

## 📋 Executive Summary

Firmware mengalami **critical design flaw** dalam task priority management yang menyebabkan BLE operations terganggu oleh RTU auto-recovery dan MQTT reconnection. Tidak ada mekanisme untuk memprioritaskan BLE command processing, menyebabkan response time degradation dari 3-5 detik menjadi 28+ detik.

---

## 🔴 Problem Statement

### Observed Behavior

**Current State:**

- ❌ BLE command response time: **~28+ seconds** (expected: 3-5 seconds)
- ❌ RTU auto-recovery berjalan **bersamaan** dengan BLE operations
- ❌ MQTT reconnection berjalan **bersamaan** dengan BLE operations
- ❌ Tidak ada task priority management
- ❌ CPU resource "berebut" antara BLE, RTU, dan MQTT

**Expected Behavior:**

- ✅ BLE command mendapat **highest priority**
- ✅ RTU auto-recovery **ter-pause** saat BLE aktif
- ✅ MQTT keepalive **ter-pause** saat BLE aktif
- ✅ BLE response time: **3-5 seconds** consistently

---

## 🔍 Root Cause Analysis

### 1. Tidak Ada BLE Priority Flag

**Evidence:**

```cpp
// Searched in all firmware files
grep -r "ble_command_active" Main/
// Result: No results found
```

**Impact:**

- Tidak ada mekanisme untuk mendeteksi BLE command sedang aktif
- Background tasks (RTU, MQTT) tidak tahu kapan harus pause
- Resource contention tidak terkelola

### 2. RTU Auto-Recovery Tidak Aware BLE State

**Current Implementation (ModbusRtuService.cpp):**

```cpp
void ModbusRtuService::readRtuDevicesLoop() {
    while (!stopPolling) {
        // Check for config changes
        if (configChangePending.load()) {
            refreshDeviceList();
            configChangePending.store(false);
        }

        // Read all RTU devices
        for (const auto& deviceId : deviceList) {
            readRtuDeviceData(deviceConfig);

            // Auto-recovery logic runs here
            if (shouldRetryDevice(deviceId)) {
                unsigned long backoffTime = calculateBackoffTime(retryCount);
                vTaskDelay(pdMS_TO_TICKS(backoffTime));
                // Retry logic...
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

**Issue:**

- ❌ Tidak ada check untuk `ble_command_active`
- ❌ Auto-recovery retry loop memblokir CPU
- ❌ Exponential backoff (100ms → 1600ms) berjalan tanpa mempedulikan BLE state

### 3. MQTT Reconnection Tidak Aware BLE State

**Current Implementation (MqttManager.cpp):**

```cpp
void MqttManager::mqttLoop() {
    while (true) {
        if (!mqttClient.connected()) {
            // Reconnect logic
            connectToMqtt();  // Takes 2-10 seconds
        }

        mqttClient.loop();

        // Publish data
        if (shouldPublish()) {
            publishDefaultMode();  // or publishCustomizeMode()
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

**Issue:**

- ❌ Tidak ada check untuk `ble_command_active`
- ❌ MQTT reconnect (2-10s) berjalan tanpa mempedulikan BLE operations
- ❌ MQTT publish menambah network overhead saat BLE transmit

### 4. BLE Manager Tidak Notify Task State

**Current Implementation (BLEManager.cpp):**

```cpp
void BLEManager::handleCompleteCommand(const char* command) {
    // Parse command
    SpiRamJsonDocument doc(commandLength + 1024);
    DeserializationError error = deserializeJson(doc, command);

    if (error) {
        sendError("Invalid JSON", "parse_error");
        return;
    }

    // Execute command via CRUDHandler
    if (crudHandler) {
        crudHandler->handleCommand(doc.as<JsonObject>(), this);
    }
}
```

**Issue:**

- ❌ Tidak ada flag `ble_command_active = true` sebelum execute
- ❌ Tidak ada flag `ble_command_active = false` setelah selesai
- ❌ Tidak ada notification ke RTU/MQTT tasks untuk pause

---

## 🎯 Recommended Solutions

### Solution 1: Implement Global BLE Priority Flag (CRITICAL)

**Implementation:**

```cpp
// File: Main/Main.ino atau BLEManager.h
// Add global atomic flag
std::atomic<bool> g_ble_command_active{false};

// BLEManager.cpp - Set flag saat command processing
void BLEManager::handleCompleteCommand(const char* command) {
    // Set BLE active flag
    g_ble_command_active.store(true);

    LOG_BLE_INFO("[BLE] Command processing started - pausing background tasks");

    // Parse and execute command
    SpiRamJsonDocument doc(commandLength + 1024);
    DeserializationError error = deserializeJson(doc, command);

    if (error) {
        sendError("Invalid JSON", "parse_error");
        g_ble_command_active.store(false);  // Clear flag on error
        return;
    }

    // Execute command
    if (crudHandler) {
        crudHandler->handleCommand(doc.as<JsonObject>(), this);
    }

    // Clear BLE active flag
    g_ble_command_active.store(false);
    LOG_BLE_INFO("[BLE] Command processing completed - resuming background tasks");
}
```

**Expected Impact:**

- BLE command processing time: **28s → 3-5s** (80% improvement)
- No more resource contention
- Predictable response time

---

### Solution 2: Pause RTU Auto-Recovery During BLE Operations (CRITICAL)

**Implementation:**

```cpp
// File: ModbusRtuService.cpp
// Add extern declaration
extern std::atomic<bool> g_ble_command_active;

void ModbusRtuService::readRtuDevicesLoop() {
    while (!stopPolling) {
        // PRIORITY CHECK: Skip if BLE is active
        if (g_ble_command_active.load()) {
            LOG_RTU_DEBUG("[RTU] BLE command active - pausing RTU polling");
            vTaskDelay(pdMS_TO_TICKS(100));  // Wait 100ms before checking again
            continue;
        }

        // Check for config changes
        if (configChangePending.load()) {
            refreshDeviceList();
            configChangePending.store(false);
        }

        // Read all RTU devices (only if BLE not active)
        for (const auto& deviceId : deviceList) {
            // Double-check BLE state before each device read
            if (g_ble_command_active.load()) {
                LOG_RTU_DEBUG("[RTU] BLE command started - aborting RTU polling");
                break;  // Exit device loop
            }

            readRtuDeviceData(deviceConfig);

            // Auto-recovery logic
            if (shouldRetryDevice(deviceId)) {
                // Skip retry if BLE is active
                if (g_ble_command_active.load()) {
                    LOG_RTU_DEBUG("[RTU] Skipping retry - BLE active");
                    break;
                }

                unsigned long backoffTime = calculateBackoffTime(retryCount);
                vTaskDelay(pdMS_TO_TICKS(backoffTime));
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

**Expected Impact:**

- RTU overhead during BLE: **~12-15s → 0s** (100% reduction)
- CPU freed for BLE operations
- No more retry loop blocking

---

### Solution 3: Pause MQTT During BLE Operations (CRITICAL)

**Implementation:**

```cpp
// File: MqttManager.cpp
// Add extern declaration
extern std::atomic<bool> g_ble_command_active;

void MqttManager::mqttLoop() {
    while (true) {
        // PRIORITY CHECK: Skip MQTT operations if BLE is active
        if (g_ble_command_active.load()) {
            LOG_MQTT_DEBUG("[MQTT] BLE command active - pausing MQTT operations");
            vTaskDelay(pdMS_TO_TICKS(100));  // Wait 100ms
            continue;
        }

        // MQTT connection check (only if BLE not active)
        if (!mqttClient.connected()) {
            // Double-check before reconnect
            if (g_ble_command_active.load()) {
                LOG_MQTT_DEBUG("[MQTT] Skipping reconnect - BLE active");
                vTaskDelay(pdMS_TO_TICKS(1000));
                continue;
            }

            connectToMqtt();
        }

        mqttClient.loop();

        // Publish data (only if BLE not active)
        if (shouldPublish() && !g_ble_command_active.load()) {
            publishDefaultMode();
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

**Expected Impact:**

- MQTT overhead during BLE: **~18-20s → 0s** (100% reduction)
- No more reconnection during BLE operations
- Network bandwidth freed for BLE transmission

---

### Solution 4: Add BLE Priority Monitoring (OPTIONAL - for debugging)

**Implementation:**

```cpp
// File: BLEManager.cpp
// Add monitoring task

void BLEManager::priorityMonitorTask(void* parameter) {
    BLEManager* manager = static_cast<BLEManager*>(parameter);

    while (true) {
        if (g_ble_command_active.load()) {
            unsigned long startTime = millis();

            // Wait for BLE command to complete
            while (g_ble_command_active.load()) {
                vTaskDelay(pdMS_TO_TICKS(100));
            }

            unsigned long duration = millis() - startTime;
            LOG_BLE_INFO("[BLE] Command completed in %lu ms", duration);

            // Alert if too slow
            if (duration > 5000) {
                LOG_BLE_WARN("[BLE] Slow command detected: %lu ms (expected < 5000ms)", duration);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// Start monitoring task in begin()
bool BLEManager::begin() {
    // ... existing code ...

    // Create priority monitor task
    xTaskCreatePinnedToCore(
        priorityMonitorTask,
        "BLE_Priority_Monitor",
        4096,
        this,
        1,  // Low priority
        &priorityMonitorTaskHandle,
        1   // Core 1
    );

    return true;
}
```

**Expected Impact:**

- Real-time monitoring of BLE command duration
- Early warning for performance issues
- Debugging aid for future optimization

---

## 📊 Performance Comparison

### Before Fix

| Metric            | Value        | Status          |
| ----------------- | ------------ | --------------- |
| BLE Response Time | ~28+ seconds | ❌ CRITICAL     |
| RTU Overhead      | ~12-15s      | ❌ BLOCKING     |
| MQTT Overhead     | ~18-20s      | ❌ BLOCKING     |
| CPU Contention    | High         | ❌ CRITICAL     |
| User Experience   | Poor         | ❌ UNACCEPTABLE |

### After Fix (Expected)

| Metric            | Value           | Status         |
| ----------------- | --------------- | -------------- |
| BLE Response Time | **3-5 seconds** | ✅ GOOD        |
| RTU Overhead      | **0s (paused)** | ✅ NO BLOCKING |
| MQTT Overhead     | **0s (paused)** | ✅ NO BLOCKING |
| CPU Contention    | Low             | ✅ GOOD        |
| User Experience   | Excellent       | ✅ ACCEPTABLE  |

**Overall Improvement: ~80-85% faster**

---

## 🧪 Test Scenarios

### Test Case 1: BLE Read Device (45 Registers)

**Steps:**

1. Configure 3 RTU devices (all failing/disconnected)
2. Enable MQTT with unstable broker
3. Connect to gateway via BLE
4. Send read device command for device with 45 registers
5. Measure response time

**Expected Result:**

- Response time: **< 5 seconds** ✅
- No RTU retry during BLE operation ✅
- No MQTT reconnect during BLE operation ✅
- Log shows "BLE command active - pausing background tasks" ✅

### Test Case 2: BLE Write Register

**Steps:**

1. Same setup as Test Case 1
2. Send write register command via BLE
3. Measure response time

**Expected Result:**

- Response time: **< 2 seconds** ✅
- RTU/MQTT paused during write ✅

### Test Case 3: BLE Backup Config (Large Payload)

**Steps:**

1. Same setup as Test Case 1
2. Send backup config command (200KB payload)
3. Measure response time

**Expected Result:**

- Response time: **< 10 seconds** ✅
- No timeout ✅
- No disconnection ✅

---

## 📝 Implementation Priority

| Priority | Solution                           | Estimated Effort | Impact     |
| -------- | ---------------------------------- | ---------------- | ---------- |
| **P0**   | Implement global BLE priority flag | 1 day            | ⭐⭐⭐⭐⭐ |
| **P0**   | Pause RTU during BLE               | 1 day            | ⭐⭐⭐⭐   |
| **P0**   | Pause MQTT during BLE              | 1 day            | ⭐⭐⭐⭐   |
| **P1**   | Add BLE priority monitoring        | 0.5 day          | ⭐⭐       |

**Total Estimated Effort:** 3-4 days for P0 fixes

---

## 🔗 Related Issues

- `BLE_PERFORMANCE_ISSUE_2026-01-20.md` - Detailed performance analysis
- `FIRMWARE_COMPREHENSIVE_ANALYSIS_2026-01-20.md` - Full firmware audit

---

## 📧 Contact

**Reporter:** Firmware Analysis Team  
**Date:** 2026-01-20  
**Firmware Version:** v1.0.6+  
**Status:** OPEN - Awaiting implementation

---

**End of Report**
