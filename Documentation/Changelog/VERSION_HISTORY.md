# Version History

**SRT-MGATE-1210 Modbus IIoT Gateway** **Firmware Release Notes**

**Developer:** SURIOTA R&D Team **Timezone:** WIB (GMT+7)

---

## Version 1.3.1 (BLE Priority Management - Critical Performance Fix)

**Release Date:** January 21, 2026 (Tuesday) **Status:** Development

### Critical Bug Fix: BLE Response Time Optimization

**Problem:** BLE CRUD operations (device read, backup, etc.) took 28+ seconds
instead of expected 3-5 seconds, causing mobile app timeouts and poor user
experience.

**Root Cause Analysis:**

1. **No BLE Priority Flag** - No mechanism existed for BLE to signal other tasks
   that a high-priority command was being processed
2. **RTU Auto-Recovery Blocking** - Modbus RTU recovery loop ran independently,
   consuming CPU during BLE operations (100ms→200ms→400ms→800ms→1600ms exponential
   backoff per device)
3. **MQTT Reconnection Blocking** - MQTT reconnection loop ran during BLE commands,
   competing for resources
4. **TCP Polling Contention** - Modbus TCP device polling continued during BLE
   operations

**Solution:** Implemented global BLE priority flag with cooperative task pausing.

### What's New

**1. Global BLE Priority Flag (Main.ino)**

```cpp
#include <atomic>
std::atomic<bool> g_bleCommandActive{false};
```

Thread-safe atomic flag that signals all background tasks when BLE is processing
a command.

**2. BLEManager Flag Management**

- Sets `g_bleCommandActive = true` at start of `handleCompleteCommand()`
- Clears `g_bleCommandActive = false` on completion or error
- Ensures flag is always cleared even on early returns

**3. RTU Service BLE Awareness**

- Added BLE check at start of `readRtuDevicesLoop()`
- Added BLE check inside device iteration loop (abort mid-poll)
- Added BLE check in `autoRecoveryLoop()`

**4. TCP Service BLE Awareness**

- Added BLE check at start of `readTcpDevicesLoop()`
- Added BLE check inside device iteration loop (abort mid-poll)

**5. MQTT Manager BLE Awareness**

- Added BLE check at start of `mqttLoop()`

### Implementation Details

When BLE command arrives:

```
1. BLEManager sets g_bleCommandActive = true
2. RTU polling loop detects flag, yields with 100ms delay
3. TCP polling loop detects flag, yields with 100ms delay
4. MQTT loop detects flag, yields with 100ms delay
5. BLE command executes with full CPU priority
6. BLEManager clears g_bleCommandActive = false
7. All loops resume normal operation
```

### Performance Impact

| Metric           | Before   | After    | Improvement |
| ---------------- | -------- | -------- | ----------- |
| BLE Response     | 28+ sec  | 3-5 sec  | 5-9x faster |
| Mobile Timeouts  | Frequent | None     | 100% fix    |
| User Experience  | Poor     | Smooth   | Significant |

### Files Modified

| File                  | Changes                                      |
| --------------------- | -------------------------------------------- |
| `Main.ino`            | Added global `g_bleCommandActive` atomic flag |
| `BLEManager.cpp`      | Set/clear BLE priority flag in command handler |
| `ModbusRtuService.cpp`| Added BLE checks in polling and recovery loops |
| `ModbusTcpService.cpp`| Added BLE checks in polling loop |
| `MqttManager.cpp`     | Added BLE check in MQTT loop |
| `ProductConfig.h`     | Version bump to 1.3.1 |

### Related Documentation

Bug report and analysis available at:
`dicky/report/bugs_issue/docs` branch in repository.

---

## Version 1.2.1 (Write Operation Bug Fix + Error Code Standardization)

**Release Date:** January 12, 2026 (Sunday) **Status:** Development

### Critical Bug Fix: TCP Write Operation

**Problem:** Write operation always failed with error 321 ("Connection failed")
even though read/streaming worked fine.

**Root Cause:** The `connected()` check in `writeRegisterValue()` was
unreliable for pooled TCP connections (as documented in code comments).

**Fix:** Removed unreliable `connected()` check, trusting
`getPooledConnection()` which already validates connection health.

```cpp
// BEFORE (buggy):
if (!pooledClient || !pooledClient->connected()) { ... }

// AFTER (fixed):
if (!pooledClient) { ... }
```

**Files Changed:**

- `ModbusTcpService.cpp` - Removed `connected()` check in write function

### Error Code Standardization

Added new write error codes (315-334) to `UnifiedErrorCodes.h`:

| Code  | Name                              | Description                            |
| ----- | --------------------------------- | -------------------------------------- |
| 315   | ERR_MODBUS_WRITE_MUTEX_TIMEOUT    | Failed to acquire mutex for write      |
| 316   | ERR_MODBUS_WRITE_DEVICE_NOT_FOUND | Device or register not found           |
| 317   | ERR_MODBUS_WRITE_READONLY         | Register is read-only (FC2/FC4)        |
| 318   | ERR_MODBUS_WRITE_NOT_WRITABLE     | Register marked as not writable        |
| 319   | ERR_MODBUS_WRITE_VALUE_BELOW_MIN  | Value below minimum limit              |
| 320   | ERR_MODBUS_WRITE_VALUE_ABOVE_MAX  | Value above maximum limit              |
| 321   | ERR_MODBUS_WRITE_CONNECTION_FAILED| Failed to connect for write            |
| 322   | ERR_MODBUS_WRITE_INVALID_FC       | Invalid write function code            |
| 323   | ERR_MODBUS_WRITE_TIMEOUT          | Write response timeout                 |
| 324   | ERR_MODBUS_WRITE_INVALID_RESPONSE | Invalid write response                 |
| 331   | ERR_MODBUS_EXCEPTION_ILLEGAL_FUNC | Modbus exception 0x01                  |
| 332   | ERR_MODBUS_EXCEPTION_ILLEGAL_ADDR | Modbus exception 0x02                  |
| 333   | ERR_MODBUS_EXCEPTION_ILLEGAL_VALUE| Modbus exception 0x03                  |
| 334   | ERR_MODBUS_EXCEPTION_DEVICE_FAIL  | Modbus exception 0x04                  |

### Files Modified

- `Main/ModbusTcpService.cpp` - Write connection fix + error codes
- `Main/ModbusRtuService.cpp` - Write error codes
- `Main/UnifiedErrorCodes.h` - New write error codes (315-334)

---

## Version 1.2.0 (Topic-Centric MQTT Subscribe - Desktop App Spec)

**Release Date:** January 12, 2026 (Sunday) **Status:** Development **Related:**
MQTT Subscribe Control Refactoring

### Breaking Change: Topic-Centric Approach

**IMPORTANT:** This version introduces a **breaking change** from v1.1.0's
register-centric approach.

| Aspect | v1.1.0 (Old) | v1.2.0 (New) |
|--------|-------------|--------------|
| Config Location | Per-register `mqtt_subscribe{}` | `server_config.custom_subscribe_mode.subscriptions[]` |
| Topic Format | Auto: `suriota/{gw}/write/{dev}/{suffix}` | User-defined custom topic |
| Topic → Registers | 1 topic → 1 register | 1 topic → N registers (multi-device) |
| Payload Format | `{"value": 25.5}` | Per-register: `{"RegA": 1, "RegB": 0}` |

### Design Principle: Topic-Centric (Desktop App Spec)

**1 Topic → N Registers** - One MQTT topic can control multiple registers across
different devices, aligning with Desktop App configuration specification.

```
# Write Command (Cloud → Gateway)
factory/hvac/setpoint                    ← User-defined topic

# Write Response (Gateway → Cloud)
factory/hvac/setpoint/response           ← Configurable response topic

# Multi-register control
factory/valves/control → ValveA, ValveB
```

### What's New

**1. Custom Subscribe Mode Configuration (server_config.json)**

```json
{
  "mqtt_config": {
    "topic_mode": "custom_subscribe",
    "custom_subscribe_mode": {
      "enabled": true,
      "subscriptions": [
        {
          "topic": "factory/hvac/setpoint",
          "qos": 1,
          "response_topic": "factory/hvac/setpoint/response",
          "registers": [
            {"device_id": "HVAC01", "register_id": "TempSetpoint"}
          ]
        },
        {
          "topic": "factory/valves/control",
          "qos": 2,
          "response_topic": "factory/valves/status",
          "registers": [
            {"device_id": "Valve01", "register_id": "ValveA"},
            {"device_id": "Valve01", "register_id": "ValveB"}
          ]
        }
      ]
    }
  }
}
```

**2. Payload Handling Logic**

| Payload | Single Register Sub | Multi-Register Sub |
|---------|--------------------|--------------------|
| `{"value": 25.5}` | Write to register | **ERROR** - require explicit values |
| `{"ValveA": 1, "ValveB": 0}` | N/A | Write 1 to ValveA, 0 to ValveB |

**3. Response Format**

```json
{
  "status": "ok",
  "topic": "factory/valves/control",
  "results": [
    {"device_id": "Valve01", "register_id": "ValveA", "status": "ok", "written_value": 1},
    {"device_id": "Valve01", "register_id": "ValveB", "status": "ok", "written_value": 0}
  ],
  "timestamp": 1736697600000
}
```

### What's Removed

1. **Per-register `mqtt_subscribe` field** - No longer supported in register config
2. **`writable_registers` BLE command** - Configuration now lives in server_config
3. **Auto-generated topic format** - Topics are now user-defined

### Files Modified

| File | Changes |
|------|---------|
| `MqttManager.h` | New structs: `MqttSubscription`, `SubscriptionRegister`; renamed methods |
| `MqttManager.cpp` | Complete rewrite of subscribe control logic to topic-centric |
| `ServerConfig.cpp` | Added `topic_mode`, `custom_subscribe_mode` config structure |
| `ConfigManager.cpp` | Removed `mqtt_subscribe` handling from register CRUD |
| `CRUDHandler.cpp` | Removed `writable_registers` command |

### Migration Guide

1. Remove `mqtt_subscribe` from all register configurations
2. Add subscriptions to `server_config.mqtt_config.custom_subscribe_mode.subscriptions[]`
3. Set `mqtt_config.topic_mode` to `"custom_subscribe"`
4. Update MQTT client to use new topic format and payload structure

### Documentation

See [MQTT_SUBSCRIBE_CONTROL.md](../Technical_Guides/MQTT_SUBSCRIBE_CONTROL.md) for
updated implementation details and testing guide.

---

## Version 1.1.0 (MQTT Subscribe Control) - DEPRECATED

**Release Date:** January 9, 2026 (Thursday) **Status:** Development **Related:**
Remote Write Control via MQTT

### Feature: Remote Write Control via MQTT Subscribe

**Background:** The v1.0.8 Write Register Support enabled local control via BLE for
writable registers (setpoints, coils, etc.). This feature extends that capability to
allow remote control via MQTT, enabling cloud/IoT platforms, SCADA systems, and
dashboards to write values to Modbus registers.

### Design Principle: Per-Register Topic

**1 Topic = 1 Register** - Each controllable register has its own MQTT topic for
safety, clarity, and easy ACL management.

```
# Write Command (Cloud → Gateway)
suriota/{gateway_id}/write/{device_id}/{topic_suffix}

# Write Response (Gateway → Cloud)
suriota/{gateway_id}/write/{device_id}/{topic_suffix}/response

# Example:
suriota/MGate1210_A3B4C5/write/D7A3F2/temp_setpoint      ← Subscribe
suriota/MGate1210_A3B4C5/write/D7A3F2/temp_setpoint/response → Publish
```

### Safety Requirements

Register must meet BOTH conditions to be controllable via MQTT:

1. `writable: true` - Register must be writable
2. `mqtt_subscribe.enabled: true` - Explicit opt-in required

### What's New

**1. Register-Level MQTT Subscribe Configuration**

```json
{
  "register_id": "R3C8D1",
  "register_name": "temp_setpoint",
  "writable": true,
  "mqtt_subscribe": {
    "enabled": true,
    "topic_suffix": "temp_setpoint",
    "qos": 1
  }
}
```

**2. Server-Level Subscribe Control Configuration**

```json
{
  "mqtt_config": {
    "subscribe_control": {
      "enabled": true,
      "topic_prefix": "suriota/MGate1210_A3B4C5/write",
      "response_enabled": true,
      "default_qos": 1
    }
  }
}
```

**3. Payload Format Options**

```
# Raw value (simplest)
25.5

# JSON with value
{"value": 25.5}

# JSON with tracking UUID
{"value": 25.5, "uuid": "550e8400-..."}
```

**4. Write Response**

```json
{
  "status": "ok",
  "device_id": "D7A3F2",
  "register_id": "R3C8D1",
  "register_name": "temp_setpoint",
  "written_value": 25.5,
  "raw_value": 255,
  "timestamp": 1704067200000
}
```

### Files Modified

| File | Changes |
|------|---------|
| `MqttManager.h` | Added subscribe control fields, `SubscribedRegister` struct, callback and methods |
| `MqttManager.cpp` | Implemented `loadSubscribeControlConfig()`, `initializeSubscriptions()`, `onMqttMessage()`, `handleWriteCommand()`, etc. |
| `ConfigManager.cpp` | Added `mqtt_subscribe` field handling in `createRegister()`, `updateRegister()`, `readRegister()` |
| `ServerConfig.cpp` | Added `subscribe_control` section in default config, `getConfig()`, `updateConfig()` |
| `CRUDHandler.cpp` | Added `writable_registers` read handler for mobile app UI |

### Integration with v1.0.8 Write Register

- Reuses existing `writeRegisterValue()` logic from ModbusRtuService/ModbusTcpService
- Same validation rules apply (min/max value, data type)
- Same reverse calibration (scale/offset) applied automatically

### Documentation

See [MQTT_SUBSCRIBE_CONTROL.md](../Technical_Guides/MQTT_SUBSCRIBE_CONTROL.md) for
full implementation details, protocol specification, and testing guide.

---

## Version 1.0.9 (BLE Cancel Command)

**Release Date:** January 9, 2026 (Thursday) **Status:** Development **Related:**
BLE Transmission Optimization

### Feature: Cancel Ongoing BLE Transmissions

**Background:** When mobile app sends a command that triggers a large response
(e.g., backup, device list with many registers), the response is sent in chunks due
to MTU limitations. If the user navigates away or sends a new command, the gateway
continues sending chunks of the old response, causing:

- Mixed responses between commands
- Wasted BLE bandwidth
- JSON parsing errors on mobile
- Poor user experience

### What's New

**1. `<CANCEL>` Command Support**

Mobile app can now send `<CANCEL>` to stop ongoing transmission:

| Command | Direction | Description |
|---------|-----------|-------------|
| `<CANCEL>` | Mobile -> Gateway | Stop current chunked transmission |
| `<ACK>` | Gateway -> Mobile | Cancel acknowledged |
| `<CANCELLED>` | Gateway -> Mobile | Transmission was cancelled (instead of `<END>`) |

**2. Atomic Cancellation Flag**

Thread-safe cancellation using `std::atomic<bool>`:

```cpp
// BLEManager.h
std::atomic<bool> transmissionCancelled{false};

// Methods
void cancelTransmission();
bool isTransmissionCancelled() const;
```

**3. Cancellation Check in sendFragmented()**

The transmission loop checks for cancellation on each chunk:

```cpp
while (i < dataLen) {
  if (transmissionCancelled.load()) {
    pResponseChar->setValue("<CANCELLED>");
    pResponseChar->notify();
    return;
  }
  // ... send chunk
}
```

### Protocol Flow

**Normal Transmission:**

```
Mobile                          Gateway
  |--- {"op":"read",...} --->     |
  |<---- chunk1 -------------     |
  |<---- chunk2 -------------     |
  |<---- ...   -------------      |
  |<---- <END> -------------      |
```

**Cancelled Transmission:**

```
Mobile                          Gateway
  |--- {"op":"read",...} --->     |
  |<---- chunk1 -------------     |
  |<---- chunk2 -------------     |
  |--- <CANCEL> ----------->      |
  |<---- <ACK> -------------      |
  |<---- <CANCELLED> -------      |  (instead of more chunks + <END>)
```

### Files Modified

| File | Changes |
|------|---------|
| `BLEManager.h` | Added `transmissionCancelled` atomic flag, `cancelTransmission()`, `isTransmissionCancelled()` |
| `BLEManager.cpp` | Handle `<CANCEL>` in `receiveFragment()`, cancellation check in `sendFragmented()`, method implementations |

### Mobile App Integration

See [FLUTTER_BLE_CANCEL_GUIDE.md](../API_Reference/FLUTTER_BLE_CANCEL_GUIDE.md)
for complete Flutter implementation guide.

**Key Integration Points:**

1. **Page dispose** - Cancel when leaving page
2. **Navigation** - Cancel before navigating to new page
3. **App lifecycle** - Cancel when app goes to background
4. **Timeout** - Auto-cancel after timeout (e.g., 30s)
5. **New command** - Cancel previous before sending new

### Testing Recommendations

- [ ] Navigate away during device list loading
- [ ] Send new command while previous is responding
- [ ] Disconnect during transmission
- [ ] Cancel at various progress points (10%, 50%, 90%)
- [ ] Rapid cancel/send cycles

---

## Version 1.0.8 (Write Register Support)

**Release Date:** January 8, 2026 (Wednesday) **Status:** Development **Related:**
Modbus Write Operations Enhancement

### Feature: Write Values to Modbus Registers

**Background:** Users need the ability to write values to Modbus devices, not just
read. This enables control scenarios like setting setpoints, toggling outputs, and
adjusting parameters via the mobile app.

### What's New

**1. New `write` Operation for Registers**

| Operation | Type     | Description                              |
| --------- | -------- | ---------------------------------------- |
| `write`   | register | Write value to holding register or coil  |

**BLE Command Example:**

```json
{
  "op": "write",
  "type": "register",
  "device_id": "D7A3F2",
  "register_id": "R3C8D1",
  "value": 25.5
}
```

**2. New Register Schema Fields**

| Field       | Type    | Default | Description                              |
| ----------- | ------- | ------- | ---------------------------------------- |
| `writable`  | boolean | false   | Enable write operations for this register|
| `min_value` | float   | -       | Minimum allowed value (optional)         |
| `max_value` | float   | -       | Maximum allowed value (optional)         |

**3. Reverse Calibration**

When writing, user values are reverse-calibrated before transmission:

```
raw_value = (user_value - offset) / scale
```

| User Value | Scale | Offset | Raw Value Sent |
| ---------- | ----- | ------ | -------------- |
| 25.5       | 0.1   | 0      | 255            |
| 100.0      | 0.01  | -10    | 11000          |
| true       | 1.0   | 0      | 1 (coil)       |

**4. Automatic Write Function Code Selection**

| Read FC | Data Type       | Write FC | Description              |
| ------- | --------------- | -------- | ------------------------ |
| FC1     | BOOL (coil)     | FC5      | Write Single Coil        |
| FC3     | 16-bit types    | FC6      | Write Single Register    |
| FC3     | 32/64-bit types | FC16     | Write Multiple Registers |

**Note:** FC2 (Discrete Inputs) and FC4 (Input Registers) are read-only by design.

### Files Modified

| File                  | Changes                                      |
| --------------------- | -------------------------------------------- |
| `CRUDHandler.h`       | Added writeHandlers map                      |
| `CRUDHandler.cpp`     | Write handler and routing logic              |
| `ModbusUtils.h/cpp`   | Reverse calibration, value conversion funcs  |
| `ModbusRtuService.cpp`| writeRegisterValue() implementation          |
| `ModbusTcpService.cpp`| writeRegisterValue() implementation          |
| `ConfigManager.cpp`   | New field handling in create/update register |
| `API.md`              | Write Register API documentation             |

### API Response

**Success Response:**

```json
{
  "status": "ok",
  "device_id": "D7A3F2",
  "register_id": "R3C8D1",
  "message": "Write successful",
  "data": {
    "register_name": "setpoint_temperature",
    "written_value": 25.5,
    "raw_value": 255,
    "function_code": 6,
    "address": 100
  }
}
```

**Error Responses:**

| Error Code | Description                                |
| ---------- | ------------------------------------------ |
| 315        | Register not writable                      |
| 316        | Value out of range (min/max validation)    |
| 317        | Unsupported data type for write            |
| 318        | Write operation failed (device error)      |

### Mobile App Integration

To enable write functionality:

1. Update register with `writable: true`
2. Optionally set `min_value` and `max_value` for validation
3. Use `op: "write"` command with target value

See [MOBILE_WRITE_FEATURE.md](../API_Reference/MOBILE_WRITE_FEATURE.md) for
complete mobile integration guide.

---

## Version 1.0.7 (Decimal Precision Control)

**Release Date:** January 8, 2026 (Wednesday) **Status:** Development **Related:**
Calibration System Enhancement

### Feature: Per-Register Decimal Precision

**Background:** Users need control over how many decimal places are displayed in
the UI and transmitted via MQTT/HTTP. Previously, values used ArduinoJson's
default precision (~7 significant digits), which could result in noisy data like
`24.560001` instead of `24.56`.

### What's New

**1. New `decimals` Field in Register Schema**

| Field      | Type    | Default | Range  | Description                        |
| ---------- | ------- | ------- | ------ | ---------------------------------- |
| `decimals` | integer | -1      | -1 to 6 | -1 = auto (no rounding), 0-6 = fixed decimal places |

**Example Configuration:**

```json
{
  "register_name": "Battery Voltage",
  "address": 40001,
  "data_type": "UINT16",
  "scale": 0.01,
  "offset": 0.0,
  "decimals": 2,
  "unit": "V"
}
```

**2. Calibration Formula with Precision**

```
final_value = round((raw_value × scale + offset), decimals)
```

| Raw Value | Scale | Offset | Decimals | Output    |
| --------- | ----- | ------ | -------- | --------- |
| 2456      | 0.01  | 0      | 2        | `24.56`   |
| 2456      | 0.01  | 0      | 1        | `24.6`    |
| 2456      | 0.01  | 0      | 0        | `25`      |
| 27.567891 | 1.0   | 0      | 3        | `27.568`  |
| 27.567891 | 1.0   | 0      | -1       | `27.567891` (auto) |

**3. Auto-Migration for Existing Registers**

Existing registers automatically receive `decimals: -1` (auto mode) during
startup migration. No configuration changes required for backward compatibility.

### Files Modified

| File                  | Changes                                      |
| --------------------- | -------------------------------------------- |
| `ConfigManager.cpp`   | Auto-migration, create/update register logic |
| `ModbusRtuService.cpp`| Apply decimal precision after calibration    |
| `ModbusTcpService.cpp`| Apply decimal precision after calibration    |
| `API.md`              | Updated register schema documentation        |

### API Changes

**Create/Update Register Request:**

```json
{
  "op": "create",
  "type": "register",
  "device_id": "D7A3F2",
  "config": {
    "register_name": "temperature",
    "address": 0,
    "data_type": "FLOAT32_BE",
    "scale": 1.0,
    "offset": 0.0,
    "decimals": 2,
    "unit": "°C"
  }
}
```

### Backward Compatibility

- ✅ Existing configs work unchanged (auto-migration adds `decimals: -1`)
- ✅ Omitting `decimals` field defaults to -1 (no rounding)
- ✅ Invalid values clamped to valid range (-1 to 6)

---

## Version 1.0.6 (Performance & Capacity Optimization)

**Release Date:** January 2, 2026 (Thursday) **Status:** Production **Related:**
ESP32-S3 Memory Architecture Optimization

### Feature: FreeRTOS Task & Memory Optimization

**Background:** Gateway designed to handle 100+ devices with 50+ registers each.
Analysis of ESP32-S3 memory architecture revealed optimization opportunities for
maximizing device capacity while improving system responsiveness.

### What's New

**1. Increased Modbus Task Stack Sizes (10KB → 12KB)**

FreeRTOS task stacks are allocated from internal DRAM (~320KB), not PSRAM. With
50+ registers per device, stack usage can spike during JSON serialization.

| Task           | Before  | After   | Headroom |
| -------------- | ------- | ------- | -------- |
| MODBUS_RTU     | 10,240B | 12,288B | +20%     |
| MODBUS_TCP     | 10,240B | 12,288B | +20%     |

**Impact:** Prevents stack overflow when polling devices with many registers.

**2. MQTT Responsiveness Optimization**

Consolidated multiple delays in MQTT task loop for faster message throughput.

| Metric         | Before               | After    | Improvement |
| -------------- | -------------------- | -------- | ----------- |
| Delays/cycle   | 3 (10+10+50ms)       | 1 (30ms) | 2.3x faster |
| Total delay    | 70ms                 | 30ms     | 57% reduced |
| Messages/sec   | ~14 msg/s            | ~33 msg/s| +135%       |

**3. Context Switch Reduction**

Increased Modbus polling delay to reduce CPU context switching overhead.

| Parameter      | Before | After  | Impact              |
| -------------- | ------ | ------ | ------------------- |
| Polling delay  | 100ms  | 150ms  | 33% fewer switches  |
| CPU overhead   | ~2.5%  | ~1.7%  | More headroom       |

**Note:** Actual data throughput unchanged - only idle loop frequency reduced.

**4. Memory Diagnostics API**

New functions for capacity planning and debugging:

```cpp
// Print comprehensive system diagnostics
MemoryRecovery::printDiagnostics();

// Get estimated additional device capacity
uint32_t capacity = MemoryRecovery::getEstimatedDeviceCapacity();
```

**Output Example:**
```
========================================
       SYSTEM DIAGNOSTICS v1.0.6
========================================

[MEMORY STATUS]
  PSRAM Free: 7,234,567 / 8,388,608 bytes (86.2%)
  DRAM Free:  45,678 / 320,000 bytes (14.3%)

[DEVICE CAPACITY]
  Current devices: 5
  Estimated additional capacity: 140 devices
  (Based on ~50KB PSRAM per device)

[RECOVERY STATUS]
  Auto-recovery: ENABLED
  Low memory events: 0
  Critical events: 0
```

### Bug Fix: Server Config Case Sensitivity

**Issue:** Mobile app sending values in different case (e.g., `"WIFI"` instead of
`"WiFi"`) was rejected by strict validation.

**Fix:** Made ALL server config validation case-insensitive:

| Field | Before (strict) | After (accepts) |
|-------|-----------------|-----------------|
| `communication.mode` | "WiFi", "ETH" | WIFI, wifi, WiFi, ETH, eth |
| `protocol` | "mqtt", "http" | MQTT, mqtt, HTTP, http |
| `publish_mode` | "default", "customize" | DEFAULT, CUSTOMIZE, etc |
| `interval_unit` | "ms", "s", "m" | MS, S, M (any case) |
| `http.method` | "GET", "POST"... | get, post, GET, POST, etc |

### Files Changed

- `Main/ModbusRtuService.cpp` - Stack 10KB→12KB, polling 100ms→150ms
- `Main/ModbusTcpService.cpp` - Stack 10KB→12KB, polling 100ms→150ms
- `Main/MqttManager.cpp` - Consolidated 3 delays into 1 (70ms→30ms)
- `Main/MemoryRecovery.h` - Added `printDiagnostics()`, `getEstimatedDeviceCapacity()`
- `Main/MemoryRecovery.cpp` - Implemented new diagnostic functions
- `Main/ServerConfig.cpp` - Case-insensitive communication mode validation
- `Main/ProductConfig.h` - Version bump to 1.0.6

### Technical Details

**ESP32-S3 Memory Architecture:**
- **DRAM (Internal):** ~320KB - Used for FreeRTOS stacks, ISR, real-time ops
- **PSRAM (External):** 8MB OPI - Used for JSON documents, buffers, queues

**Why Task Stacks Can't Use PSRAM:**
- FreeRTOS `xTaskCreate()` allocates from internal heap only
- PSRAM access latency incompatible with context switching requirements
- JSON documents already use PSRAM via `SpiRamJsonDocument` (no change needed)

**Capacity Calculation:**
- ~50KB PSRAM per device (with 50 registers)
- ~2KB DRAM per device (task overhead)
- Limiting factor: Usually PSRAM (8MB ÷ 50KB = ~160 devices theoretical max)

---

## Version 1.0.5 (Server Config Validation)

**Release Date:** January 2, 2026 (Thursday) **Status:** Production **Related:**
Mobile App Error Handling Enhancement

### Feature: Comprehensive Server Configuration Validation

**Background:** Previously, server_config updates only checked if
`communication` and `protocol` fields existed, without validating their values.
This caused:

- Mobile app received `success` even with invalid configurations
- Users didn't know which fields were required vs optional
- Errors only appeared when device tried to use invalid config (e.g., WiFi with
  no SSID)

### What's New

**1. Enhanced Validation with Detailed Error Messages**

Firmware now validates all server config fields and returns specific error
information:

```json
{
  "status": "error",
  "error_code": 509,
  "domain": "CONFIG",
  "severity": "ERROR",
  "message": "WiFi SSID is required when WiFi is enabled",
  "field": "wifi.ssid",
  "suggestion": "Provide a valid WiFi network name"
}
```

**2. Validation Rules Implemented**

| Section           | Field                            | Validation                                                 |
| ----------------- | -------------------------------- | ---------------------------------------------------------- |
| **Communication** | `mode`                           | Required. Must be "ETH" or "WiFi"                          |
| **WiFi**          | `enabled`                        | Must be `true` when mode is "WiFi"                         |
| **WiFi**          | `ssid`                           | Required when enabled. Max 32 chars                        |
| **WiFi**          | `password`                       | Min 8 chars if provided (empty = open network)             |
| **Ethernet**      | `enabled`                        | Must be `true` when mode is "ETH"                          |
| **Ethernet**      | `use_dhcp`                       | If `false`, static_ip/gateway/subnet required              |
| **Ethernet**      | `static_ip`, `gateway`, `subnet` | Valid IPv4 format (xxx.xxx.xxx.xxx)                        |
| **MQTT**          | `broker_address`                 | Required when enabled                                      |
| **MQTT**          | `broker_port`                    | 1-65535                                                    |
| **MQTT**          | `keep_alive`                     | 30-3600 seconds                                            |
| **MQTT**          | `publish_mode`                   | "default" or "customize"                                   |
| **MQTT**          | `interval_unit`                  | "ms", "s", or "m"                                          |
| **HTTP**          | `endpoint_url`                   | Required when enabled. Must start with http:// or https:// |
| **HTTP**          | `method`                         | GET, POST, PUT, PATCH, or DELETE                           |
| **HTTP**          | `timeout`                        | 1000-30000 ms                                              |
| **HTTP**          | `retry`                          | 0-10                                                       |

**3. New Response Fields for Mobile App**

- `field` - Which field caused the error (e.g., "wifi.ssid",
  "ethernet.static_ip")
- `suggestion` - Human-readable suggestion to fix the error

### Files Changed

- `Main/ServerConfig.h` - Added `ConfigValidationResult` struct and validation
  methods
- `Main/ServerConfig.cpp` - Implemented `validateConfigEnhanced()` with all
  validations
- `Main/CRUDHandler.cpp` - Enhanced error response with field-specific details
- `Main/ProductConfig.h` - Version bump to 1.0.5

### Mobile App Integration

**Handle validation errors:**

```dart
void handleServerConfigResponse(Map<String, dynamic> response) {
  if (response['status'] == 'error') {
    String field = response['field'] ?? '';
    String message = response['message'] ?? 'Unknown error';
    String suggestion = response['suggestion'] ?? '';

    // Highlight specific field in UI if 'field' is provided
    if (field.isNotEmpty) {
      highlightFieldError(field, message);
    }

    showErrorDialog(message: message, suggestion: suggestion);
  }
}
```

### Example Error Scenarios

| Invalid Config                     | Error Message                                            | Field                      |
| ---------------------------------- | -------------------------------------------------------- | -------------------------- |
| WiFi mode but enabled=false        | "WiFi must be enabled when communication mode is 'WiFi'" | wifi.enabled               |
| WiFi enabled but no SSID           | "WiFi SSID is required when WiFi is enabled"             | wifi.ssid                  |
| ETH mode, DHCP=false, no static IP | "Static IP is required when DHCP is disabled"            | ethernet.static_ip         |
| Invalid IP format "192.168.1"      | "Invalid static IP format. Use xxx.xxx.xxx.xxx"          | ethernet.static_ip         |
| MQTT enabled but no broker         | "MQTT broker address is required when MQTT is enabled"   | mqtt_config.broker_address |
| HTTP URL without http://           | "HTTP endpoint must start with http:// or https://"      | http_config.endpoint_url   |

### Enhancement: Faster Modbus Auto-Recovery

**Background:** When a Modbus device disconnects (cable unplugged, device
powered off), the gateway disables the device after 5 failed retries.
Previously, auto-recovery only checked every **5 minutes**, which was too slow
for many use cases.

**Change:** Reduced recovery interval from **5 minutes** to **30 seconds**.

| Protocol       | Retry Cycle Time | Recovery Interval | Worst Case Recovery |
| -------------- | ---------------- | ----------------- | ------------------- |
| **Modbus RTU** | ~3.1 seconds     | 30 seconds        | ~33 seconds         |
| **Modbus TCP** | ~62 seconds      | 30 seconds        | ~92 seconds         |

**RTOS Safety Analysis:**

- Recovery task is lightweight (iterates device list, resets flags)
- No memory allocation in recovery loop
- CPU usage < 0.1% per 30-second cycle
- Thread-safe via recursive mutex protection

**Files Changed:**

- `Main/ModbusRtuService.cpp` - `RECOVERY_INTERVAL_MS = 30000`
- `Main/ModbusTcpService.cpp` - `RECOVERY_INTERVAL_MS = 30000`

**User Impact:** When cable is reconnected or device is powered back on, polling
resumes automatically within ~30 seconds (instead of waiting up to 5 minutes).

---

## Version 1.0.4 (Device Summary Connection Identifiers)

**Release Date:** December 29, 2025 (Sunday) **Status:** Production **Related:**
BUG_001_SLAVE_ID_VALIDATION (Mobile App)

### Bug Fix: Device Summary Missing Connection Identifiers

**Background:** Mobile app's slave_id validation was rejecting valid
configurations because `devices_summary` and `devices_with_registers` (minimal
mode) did not include `ip` and `serial_port` fields. Without these fields,
mobile app couldn't determine if slave_id collision was on the same
bus/endpoint.

**Root Cause:**

- `getDevicesSummary()` only returned: `device_id`, `device_name`, `protocol`,
  `slave_id`, `register_count`
- `getAllDevicesWithRegisters(minimal=true)` did not include `ip` field

**Impact:** Mobile app showed "Slave ID already exists" error even when:

- RTU devices were on **different serial ports** (different RS-485 bus)
- TCP devices had **different IP addresses** (different endpoints)
- Devices used **different protocols** (RTU vs TCP)

### Fix Applied

**1. `getDevicesSummary()` now includes:**

```json
{
  "device_id": "D7A3F2",
  "device_name": "Meter RTU",
  "protocol": "RTU",
  "slave_id": 10,
  "serial_port": 1, // NEW - for RTU devices
  "register_count": 5
}
```

```json
{
  "device_id": "B8C4E1",
  "device_name": "PLC TCP",
  "protocol": "TCP",
  "slave_id": 10,
  "ip": "192.168.1.100", // NEW - for TCP devices
  "register_count": 3
}
```

**2. `getAllDevicesWithRegisters(minimal=true)` now includes `ip` field**

### Correct Validation Logic (Mobile App)

Per Modbus standard, slave_id uniqueness should be checked:

- **RTU:** Per `serial_port` (RS-485 bus)
- **TCP:** Per `ip` (endpoint)
- **Cross-protocol:** Always allowed (RTU and TCP are independent)

| Scenario                               | Before   | After    |
| -------------------------------------- | -------- | -------- |
| RTU Port 1 + RTU Port 2, same slave_id | ❌ Error | ✅ OK    |
| RTU + TCP, same slave_id               | ❌ Error | ✅ OK    |
| TCP IP1 + TCP IP2, same slave_id       | ❌ Error | ✅ OK    |
| RTU Port 1 + RTU Port 1, same slave_id | ❌ Error | ❌ Error |
| TCP same IP, same slave_id             | ❌ Error | ❌ Error |

### Files Changed

- `Main/ConfigManager.cpp` - Added `ip` and `serial_port` to summary responses
- `Main/ProductConfig.h` - Version bump to 1.0.4

### Mobile App Action Required

Update `_isSlaveIdDuplicate()` in `form_setup_device_screen.dart` to use:

- `device['serial_port']` for RTU protocol comparison
- `device['ip']` for TCP protocol comparison

See `BUG_001_SLAVE_ID_VALIDATION.md` for detailed implementation guide.

---

## Version 1.0.3 (Config Transfer Progress)

**Release Date:** December 27, 2025 (Friday) **Status:** Production (⚠️ BREAKING
CHANGE - Mobile App Update Required)

### Feature: Config Transfer Progress Notifications - ACTIVATED

**Background:** Unlike OTA which has real-time progress notifications, config
backup/restore operations had no progress reporting to the mobile app.

**Status:** ✅ Progress notifications now ACTIVE!

### ⚠️ BREAKING CHANGE

Mobile app **MUST** be updated to handle new notification types before using
this firmware version. Without update, backup/restore operations will have
**corrupted JSON responses**.

### New Notification Types

**1. config_download_progress** (during backup/export):

```json
{
  "type": "config_download_progress",
  "percent": 45,
  "bytes_sent": 5000,
  "total_bytes": 11000
}
```

- Sent every 10% during large response transmission (>5KB)
- Mobile app must filter this from response buffer

**2. config_restore_progress** (during restore/import):

```json
{
  "type": "config_restore_progress",
  "step": "devices",
  "current": 1,
  "total": 3,
  "percent": 33
}
```

- Sent at each restore step: `devices` → `server_config` → `logging_config`
- Mobile app must filter this from response buffer

**3. config_upload_progress** (NOT sent by firmware):

```json
{
  "type": "config_upload_progress",
  "percent": 60,
  "bytes_received": 6000,
  "total_expected": 10000
}
```

- Function exists but NOT called - mobile app should track upload progress
  locally
- Mobile app knows bytes sent vs total payload size

### Mobile App Changes Required

Add handler in BLE notification receiver (like existing `ota_progress`):

```dart
void _handleBleNotification(List<int> data) {
  final jsonStr = utf8.decode(data);

  // Filter progress notifications - don't add to response buffer
  if (jsonStr.startsWith('{"type":"config_download_progress"') ||
      jsonStr.startsWith('{"type":"config_restore_progress"') ||
      jsonStr.startsWith('{"type":"config_upload_progress"')) {
    _handleConfigProgress(jsonDecode(jsonStr));
    return; // Don't add to response buffer!
  }

  // Existing ota_progress handler
  if (jsonStr.startsWith('{"type":"ota_progress"')) {
    _handleOtaProgress(jsonDecode(jsonStr));
    return;
  }

  // Regular response - add to buffer
  _responseBuffer.addAll(data);
}
```

### Files Changed

- `Main/BLEManager.cpp` - Activated `sendConfigDownloadProgress()` in
  `sendFragmented()`
- `Main/CRUDHandler.cpp` - Added `sendConfigRestoreProgress()` calls in restore
  handler
- `Main/ProductConfig.h` - Version bump to 1.0.3

---

## Version 1.0.2 (Standardized Error Responses)

**Release Date:** December 28, 2025 (Saturday) **Status:** Production

### Feature: Standardized Error Response Format

**Background:** BLE CRUD responses previously used inconsistent error formats
with string-based error codes. Mobile apps couldn't programmatically handle
errors reliably.

**Solution:** Implemented `ErrorResponseHelper` utility that creates consistent
error responses with:

- **Numeric error_code** (0-699) from `UnifiedErrorCodes.h`
- **Domain classification** (NETWORK, MQTT, BLE, MODBUS, MEMORY, CONFIG, SYSTEM)
- **Severity level** (INFO, WARN, ERR, CRIT)
- **Human-readable message**
- **Recovery suggestion** (when available)

### New Error Response Format

**Before (v1.0.1):**

```json
{
  "status": "error",
  "message": "Device not found",
  "type": "device"
}
```

**After (v1.0.2):**

```json
{
  "status": "error",
  "error_code": 501,
  "domain": "CONFIG",
  "severity": "ERROR",
  "message": "Device not found",
  "suggestion": "Create new configuration",
  "type": "device"
}
```

### Error Code Ranges

| Domain  | Range   | Examples                   |
| ------- | ------- | -------------------------- |
| NETWORK | 0-99    | WiFi/Ethernet connectivity |
| MQTT    | 100-199 | Broker communication       |
| BLE     | 200-299 | Bluetooth Low Energy       |
| MODBUS  | 300-399 | RTU/TCP devices            |
| MEMORY  | 400-499 | PSRAM allocation           |
| CONFIG  | 500-599 | Configuration/Storage      |
| SYSTEM  | 600-699 | System health              |

### Files Added

- `Main/ErrorResponseHelper.h` - Helper functions for standardized error
  responses

### Files Changed

- `Main/BLEManager.h` - Added `sendError(UnifiedErrorCode, ...)` overloads
- `Main/BLEManager.cpp` - Implemented new sendError methods
- `Main/CRUDHandler.cpp` - Updated key error responses to use new format
- `Main/OTACrudBridge.cpp` - Updated OTA error responses
- `Documentation/API_Reference/API.md` - Documented new error format

### Mobile App Integration

```dart
void handleError(Map<String, dynamic> response) {
  if (response['status'] == 'error') {
    final errorCode = response['error_code'] as int;
    final domain = response['domain'] as String;
    final severity = response['severity'] as String;

    // Programmatic handling
    switch (errorCode) {
      case 7:   // ERR_NET_NO_NETWORK_AVAILABLE
        showNetworkError();
        break;
      case 301: // ERR_MODBUS_DEVICE_TIMEOUT
        showDeviceOfflineWarning();
        break;
      case 504: // ERR_CFG_SAVE_FAILED
        showSaveError();
        break;
    }

    // UI styling by severity
    if (severity == 'CRIT') {
      showCriticalAlert(response['message']);
    }
  }
}
```

### Backward Compatibility

- The `type` field is preserved for backward compatibility
- Old error responses still work but lack new fields
- Mobile apps can check for `error_code` presence for progressive enhancement

---

### Previous v1.0.2 Features (Config Transfer Progress - Preparation)

Functions implemented but not activated in v1.0.2. See v1.0.3 for activated
version.

**Functions Added:**

```cpp
void sendConfigDownloadProgress(uint8_t percent, size_t bytesSent, size_t totalBytes);
void sendConfigUploadProgress(uint8_t percent, size_t bytesReceived, size_t totalExpected);
void sendConfigRestoreProgress(const String& step, uint8_t currentStep, uint8_t totalSteps);
```

---

## Version 1.0.1 (Patch Release)

**Release Date:** December 27, 2025 (Friday) **Status:** Production Patch

### Critical Bug Fix

**Issue:** Modbus TCP/RTU config changes (IP address, slave ID, etc.) took ~2
minutes to take effect.

**Symptoms:**

- Config change notification received immediately
- Old config still used for polling for ~2 minutes
- Logs showed "ERROR" connecting to old IP until device refresh

**Root Cause:**

- `configChangePending` flag only checked BETWEEN devices (line 324)
- NOT checked BETWEEN registers during device polling
- With 45 registers × 3 second timeout = **135 seconds delay** before config
  refresh

**Fix:**

- Added `configChangePending.load()` check inside register polling loop
- Both `ModbusTcpService.cpp` and `ModbusRtuService.cpp` patched
- Config changes now detected within 1 register poll cycle (~3 seconds max)

**Files Changed:**

- `Main/ModbusTcpService.cpp` - Added config check in register loop
- `Main/ModbusRtuService.cpp` - Added config check in register loop
- `Main/ProductConfig.h` - Version bump to 1.0.1

---

## Version 1.0.0 (Production Release)

**Release Date:** December 27, 2025 (Friday) **Status:** Production Ready

### Overview

First official production release of the SRT-MGATE-1210 Industrial IoT Gateway
firmware. This release consolidates all development work from the Alpha/Beta
phases (v0.x.x) into a stable, production-ready firmware.

---

### Core Features

#### 1. Modbus Protocol Support

- **Modbus RTU** - Dual RS485 ports with configurable baud rate, parity, stop
  bits
- **Modbus TCP** - Ethernet/WiFi with connection pooling and auto-reconnect
- **40+ Data Types** - INT16, UINT32, FLOAT32, SWAP variants, BCD, ASCII, etc.
- **Calibration** - Per-register scale and offset support

#### 2. Cloud Connectivity

- **MQTT** - TLS support, retain flag, configurable QoS, auto-reconnect
- **HTTP/HTTPS** - REST API integration with certificate validation
- **Data Buffering** - Queue-based message buffering during network outages

#### 3. BLE Configuration Interface

- **CRUD API** - Full device/register/server configuration via BLE
- **OTA Updates** - Signed firmware updates from GitHub (public/private repos)
- **Backup/Restore** - Complete configuration export/import
- **Large Response Support** - Up to 200KB fragmented responses

#### 4. Network Management

- **Dual Network** - WiFi + Ethernet with automatic failover
- **Network Status API** - Real-time connectivity monitoring
- **DNS Resolution** - Hostname-based server configuration

#### 5. Memory Optimization

- **PSRAMString** - Unified PSRAM-based string allocation
- **Memory Recovery** - 4-tier automatic memory management
- **Shadow Copy Pattern** - Reduced file system I/O

#### 6. Production Features

- **Two-Tier Logging** - Compile-time + runtime log level control
- **Error Codes** - Unified error code system (7 domains, 60+ codes)
- **Atomic File Writes** - WAL pattern for crash-safe configuration

---

### Hardware Configuration

| Component | Specification               |
| --------- | --------------------------- |
| MCU       | ESP32-S3 (240MHz dual-core) |
| SRAM      | 512KB                       |
| PSRAM     | 8MB                         |
| Flash     | 16MB                        |
| RS485     | 2 ports (MAX485)            |
| Ethernet  | W5500                       |
| BLE       | BLE 5.0                     |

---

### Version Numbering

**Format:** `MAJOR.MINOR.PATCH`

- **MAJOR:** Breaking changes (API incompatible)
- **MINOR:** New features (backward compatible)
- **PATCH:** Bug fixes and optimizations

---

### Development History

The development phase (Alpha/Beta testing) is documented in:

- `DEVELOPMENT_HISTORY_ARCHIVE.md` - Complete development changelog

Key milestones from development:

- PSRAMString unification for memory optimization
- Shadow cache synchronization for config changes
- MQTT client ID collision fix
- BLE MTU negotiation timeout handling
- TCP connection pooling optimization
- Production mode logging system

---

### Quality Metrics

| Category          | Score | Details                          |
| ----------------- | ----- | -------------------------------- |
| Logging System    | 97%   | Two-tier with 15+ module macros  |
| Thread Safety     | 96%   | Mutex-protected shared resources |
| Memory Management | 95%   | PSRAM-first with DRAM fallback   |
| Error Handling    | 93%   | Unified codes, 7 domains         |
| Testing Coverage  | 70%   | Integration tests, simulators    |

**Overall Score:** 91/100 (Production Ready)

---

### Related Documentation

- [API Reference](../API_Reference/API.md) - Complete BLE API documentation
- [Modbus Data Types](../Technical_Guides/MODBUS_DATATYPES.md) - 40+ supported
  types
- [Protocol Guide](../Technical_Guides/PROTOCOL.md) - BLE/Modbus protocol specs
- [Troubleshooting](../Technical_Guides/TROUBLESHOOTING.md) - Common issues and
  solutions

---

**Made by SURIOTA R&D Team** | Industrial IoT Solutions **Contact:**
support@suriota.com | www.suriota.com
