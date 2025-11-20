# LOG SYSTEM MIGRATION GUIDE

## Overview
This guide shows how to migrate existing logging statements to the new **granular log level system**.

---

## Quick Reference Table

| Old Format | New Format | Log Level | When to Use |
|------------|------------|-----------|-------------|
| `Serial.printf("[RTU] ERROR: %s\n", msg)` | `LOG_RTU_ERROR("ERROR: %s\n", msg)` | ERROR | Critical failures, crashes |
| `Serial.printf("[RTU] Warning: %s\n", msg)` | `LOG_RTU_WARN("Warning: %s\n", msg)` | WARN | Potential issues, degraded operation |
| `Serial.printf("[RTU] Polling device %s\n", id)` | `LOG_RTU_VERBOSE("Polling device %s\n", id)` | VERBOSE | Per-operation details |
| `Serial.printf("[RTU] Device %s disabled\n", id)` | `LOG_RTU_INFO("Device %s disabled\n", id)` | INFO | State changes, key events |
| `Serial.printf("[TCP] Connected to %s\n", ip)` | `LOG_TCP_INFO("Connected to %s\n", ip)` | INFO | Connection status |
| `Serial.printf("[MQTT] Published %d regs\n", n)` | `LOG_MQTT_INFO("Published %d regs\n", n)` | INFO | Data transmission |
| `Serial.printf("[BATCH] Started tracking...\n")` | `LOG_BATCH_DEBUG("Started tracking...\n")` | DEBUG | Internal state tracking |

---

## Module-Specific Macros

### RTU Service
```cpp
LOG_RTU_ERROR()    // Modbus read failures, timeout errors
LOG_RTU_WARN()     // Retry attempts, backoff warnings
LOG_RTU_INFO()     // Device enable/disable, connection status
LOG_RTU_DEBUG()    // Failure state changes, backoff calculations
LOG_RTU_VERBOSE()  // Per-device polling, per-register reads
```

### TCP Service
```cpp
LOG_TCP_ERROR()    // Connection failures, socket errors
LOG_TCP_WARN()     // Timeout warnings, retry attempts
LOG_TCP_INFO()     // Connection established, device polling
LOG_TCP_DEBUG()    // Transaction ID, response parsing
LOG_TCP_VERBOSE()  // Per-register reads, buffer operations
```

### MQTT Manager
```cpp
LOG_MQTT_ERROR()   // Publish failures, connection lost
LOG_MQTT_WARN()    // Large payloads, queue near full
LOG_MQTT_INFO()    // Connected, published data summary
LOG_MQTT_DEBUG()   // Batch waiting, persistent queue operations
LOG_MQTT_VERBOSE() // Individual message enqueueing
```

### Batch Manager
```cpp
LOG_BATCH_ERROR()  // Mutex failures
LOG_BATCH_WARN()   // Batch timeout warnings
LOG_BATCH_INFO()   // Batch started, batch complete
LOG_BATCH_DEBUG()  // Increment operations, clear operations
```

---

## Migration Examples

### Example 1: ModbusRtuService.cpp

#### BEFORE:
```cpp
Serial.printf("[RTU] Polling device %s (Slave:%d Port:%d Baud:%d)\n",
              deviceId.c_str(), slaveId, serialPort, baudRate);
```

#### AFTER:
```cpp
LOG_RTU_VERBOSE("Polling device %s (Slave:%d Port:%d Baud:%d)\n",
                deviceId.c_str(), slaveId, serialPort, baudRate);
```

**Rationale:** This is per-device polling (happens every ~1s), so it should be VERBOSE level to avoid spam.

---

### Example 2: Error Handling (ModbusRtuService.cpp:254-256)

#### BEFORE:
```cpp
if (!isDeviceEnabled(deviceId))
{
  Serial.printf("[RTU] Device %s is disabled, skipping read\n", deviceId.c_str());
  return;
}
```

#### AFTER:
```cpp
if (!isDeviceEnabled(deviceId))
{
  LOG_RTU_INFO("Device %s is disabled, skipping read\n", deviceId.c_str());
  return;
}
```

**Rationale:** Device state changes are important (INFO level), but not errors.

---

### Example 3: Retry Backoff (ModbusRtuService.cpp:265)

#### BEFORE:
```cpp
Serial.printf("[RTU] Device %s retry backoff not elapsed, skipping\n", deviceId.c_str());
```

#### AFTER:
```cpp
LOG_RTU_DEBUG("Device %s retry backoff not elapsed, skipping\n", deviceId.c_str());
```

**Rationale:** Internal retry logic details (DEBUG level) - not needed in production.

---

### Example 4: Success/Failure Summary (ModbusRtuService.cpp:520-522)

#### BEFORE:
```cpp
resetDeviceFailureState(deviceId);
Serial.printf("[RTU] Device %s: Read successful, failure state reset\n", deviceId.c_str());
```

#### AFTER:
```cpp
resetDeviceFailureState(deviceId);
LOG_RTU_INFO("Device %s: Read successful, failure state reset\n", deviceId.c_str());
```

**Rationale:** Recovery from failure is important (INFO level) - shows system health.

---

### Example 5: Critical Error (ModbusRtuService.cpp:535-536)

#### BEFORE:
```cpp
// All registers failed - handle read failure
Serial.printf("[RTU] Device %s: All %d register reads failed\n", deviceId.c_str(), failedRegisterCount);
```

#### AFTER:
```cpp
// All registers failed - handle read failure
LOG_RTU_ERROR("Device %s: All %d register reads failed\n", deviceId.c_str(), failedRegisterCount);
```

**Rationale:** Complete device failure is critical (ERROR level) - requires attention.

---

### Example 6: MQTT Batch Waiting (MqttManager.cpp:478-481)

#### BEFORE:
```cpp
static unsigned long lastBatchWarn = 0;
if (now - lastBatchWarn > 10000) { // Log every 10s to avoid spam
  Serial.println("[MQTT] Waiting for device batch to complete...");
  lastBatchWarn = now;
}
```

#### AFTER:
```cpp
static LogThrottle batchWaitingThrottle(60000); // Log every 60s
if (batchWaitingThrottle.shouldLog()) {
  LOG_MQTT_DEBUG("Waiting for device batch to complete...\n");
}
```

**Rationale:** Use LogThrottle class + DEBUG level for repetitive messages.

---

### Example 7: TCP Connection (ModbusTcpService - similar pattern)

#### BEFORE:
```cpp
Serial.printf("[TCP] Connected to %s:%d\n", ip.c_str(), port);
```

#### AFTER:
```cpp
LOG_TCP_INFO("Connected to %s:%d\n", ip.c_str(), port);
```

---

### Example 8: Batch Tracking (DeviceBatchManager.h:86-87)

#### BEFORE:
```cpp
Serial.printf("[BATCH] Started tracking %s: expecting %d registers\n",
              deviceId.c_str(), registerCount);
```

#### AFTER:
```cpp
LOG_BATCH_INFO("Started tracking %s: expecting %d registers\n",
               deviceId.c_str(), registerCount);
```

---

## Log Level Guidelines

### ERROR (Production - Always Visible)
- **Use for:** Critical failures, crashes, data loss
- **Examples:**
  - Device completely offline
  - MQTT publish failed after retries
  - Memory allocation failures
  - File system errors
- **Action Required:** User intervention needed

### WARN (Production - Recommended)
- **Use for:** Degraded operation, potential issues
- **Examples:**
  - Retry attempts active
  - Low memory warning (>20KB)
  - Queue near capacity (>80%)
  - Slow response times
- **Action Required:** Monitor situation

### INFO (Production - Default)
- **Use for:** Key events, state changes, summaries
- **Examples:**
  - Connection established/lost
  - Device enabled/disabled
  - Batch completed (summary)
  - MQTT published (summary)
- **Action Required:** Informational only

### DEBUG (Development Only)
- **Use for:** Internal state, algorithm details
- **Examples:**
  - Batch increment operations
  - Failure state updates
  - Queue depth checks
  - Backoff calculations
- **Action Required:** Development debugging

### VERBOSE (Development Only)
- **Use for:** Per-operation details, high-frequency events
- **Examples:**
  - Per-device polling
  - Per-register reads
  - Per-message enqueueing
  - MTU negotiation details
- **Action Required:** Deep debugging

---

## Throttling Patterns

### Pattern 1: Static LogThrottle
```cpp
static LogThrottle throttle(60000); // 60s interval
if (throttle.shouldLog()) {
  LOG_MQTT_DEBUG("Still waiting...\n");
}
```

### Pattern 2: Class Member LogThrottle
```cpp
// In .h file
class MyService {
private:
  LogThrottle connectThrottle;

public:
  MyService() : connectThrottle(30000) {} // 30s interval
};

// In .cpp file
if (connectThrottle.shouldLog()) {
  LOG_NET_WARN("Connection retry attempt...\n");
}
```

---

## Testing Commands

### In Main.ino setup():
```cpp
// Set log level at startup
setLogLevel(LOG_INFO); // Production default

// Print status
printLogLevelStatus();

// Test all levels
LOG_RTU_ERROR("Test ERROR - should always show\n");
LOG_RTU_WARN("Test WARN - shows at INFO level\n");
LOG_RTU_INFO("Test INFO - shows at INFO level\n");
LOG_RTU_DEBUG("Test DEBUG - hidden at INFO level\n");
LOG_RTU_VERBOSE("Test VERBOSE - hidden at INFO level\n");

// Change level at runtime
setLogLevel(LOG_VERBOSE); // Enable all logs for testing
```

### Via BLE CRUD (future enhancement):
```json
{"op":"update","type":"config","key":"log_level","value":3}
```

---

## Migration Checklist

### ModbusRtuService.cpp
- [ ] Line 255: Device disabled → `LOG_RTU_INFO`
- [ ] Line 265: Retry backoff → `LOG_RTU_DEBUG`
- [ ] Line 280: Polling device → `LOG_RTU_VERBOSE`
- [ ] Line 364: Register error → `LOG_RTU_WARN`
- [ ] Line 522: Success recovery → `LOG_RTU_INFO`
- [ ] Line 536: All failed → `LOG_RTU_ERROR`

### ModbusTcpService.cpp
- [ ] Connection logs → `LOG_TCP_INFO`
- [ ] Polling device → `LOG_TCP_VERBOSE`
- [ ] Read errors → `LOG_TCP_ERROR`
- [ ] Transaction details → `LOG_TCP_DEBUG`

### MqttManager.cpp
- [ ] Connection status → `LOG_MQTT_INFO`
- [ ] Publish success → `LOG_MQTT_INFO`
- [ ] Publish failure → `LOG_MQTT_ERROR`
- [ ] Batch waiting → `LOG_MQTT_DEBUG` (throttled)
- [ ] Queue operations → `LOG_MQTT_VERBOSE`

### DeviceBatchManager.h
- [ ] Batch started → `LOG_BATCH_INFO`
- [ ] Batch complete → `LOG_BATCH_INFO`
- [ ] Increment operations → `LOG_BATCH_DEBUG`
- [ ] Mutex errors → `LOG_BATCH_ERROR`

---

## Expected Output Reduction

### Before (LOG_VERBOSE):
```
[RTU] Polling device D7227b (Slave:1 Port:2 Baud:9600)
[BATCH] Started tracking D7227b: expecting 48 registers
[DATA] D7227b:
  L1: Temp_Zone_1:24.0degC | ...
[RTU] Device D7227b: Read successful, failure state reset
[BATCH] Device D7227b COMPLETE (48 success, 0 failed, 48/48 total, took 47959 ms)
[MQTT] Waiting for device batch to complete...
[MQTT] Waiting for device batch to complete...
[MQTT] Default Mode: Published 48 registers from 1 devices to v1/devices/me/telemetry/gwsrt (2.3 KB)
[BATCH] Cleared batch status for device D7227b
```
**Total:** 9 lines per cycle

### After (LOG_INFO - Production):
```
[INFO][BATCH] Started tracking D7227b: expecting 48 registers
[DATA] D7227b:
  L1: Temp_Zone_1:24.0degC | ...
[INFO][RTU] Device D7227b: Read successful, failure state reset
[INFO][BATCH] Device D7227b COMPLETE (48 success, 0 failed, 48/48 total, took 47959 ms)
[INFO][MQTT] Default Mode: Published 48 registers from 1 devices to v1/devices/me/telemetry/gwsrt (2.3 KB)
```
**Total:** 5 lines per cycle (~45% reduction)

---

## Backward Compatibility

All existing `Serial.printf()` statements will continue to work. Migration is **optional** and can be done **incrementally**.

---

## Next Steps

1. ✅ Apply migrations to **ModbusRtuService.cpp**
2. ✅ Apply migrations to **ModbusTcpService.cpp**
3. ✅ Apply migrations to **MqttManager.cpp**
4. ✅ Apply migrations to **DeviceBatchManager.h**
5. Test with different log levels
6. Add BLE CRUD command for runtime log level control
