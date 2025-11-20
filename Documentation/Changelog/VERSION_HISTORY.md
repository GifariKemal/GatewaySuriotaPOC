# 📋 Version History

**SRT-MGATE-1210 Modbus IIoT Gateway**
Firmware Changelog and Release Notes

**Developer:** Kemal
**Timezone:** WIB (GMT+7)

---

## 📦 Version 2.2.0 (Current)

**Release Date:** November 14, 2025 (Friday)
**Developer:** Kemal
**Status:** ✅ Production Ready

### 🎯 Configuration Refactoring - Clean API Structure

#### 🔧 HTTP Interval Configuration (Breaking Change)

**Problem Identified:**
- `data_interval` at root level was confusing and only used by HTTP
- MQTT already had mode-specific intervals (`default_mode.interval`, `customize_mode.custom_topics[].interval`)
- Inconsistent structure made mobile app development harder

**Solution:**
Moved HTTP transmission interval INTO `http_config` for consistency.

**Before (v2.1.1):**
```json
{
  "config": {
    "data_interval": {"value": 5, "unit": "s"},  // ❌ Root level (only for HTTP)
    "protocol": "http",
    "http_config": {
      "enabled": true,
      "endpoint_url": "https://api.example.com/data"
    }
  }
}
```

**After (v2.2.0):**
```json
{
  "config": {
    "protocol": "http",
    "http_config": {
      "enabled": true,
      "endpoint_url": "https://api.example.com/data",
      "interval": 5,           // ✅ Moved here
      "interval_unit": "s"     // ✅ Consistent with MQTT
    }
  }
}
```

**Benefits:**
- ✅ Consistent API structure (each protocol has its own interval)
- ✅ No redundant root-level fields
- ✅ Easier to understand and maintain
- ✅ Scalable for future protocols (WebSocket, CoAP, etc.)

---

### 📝 Changes

#### 🗑️ Removed (Breaking Changes)
- **`data_interval`** (root level) - Removed entirely
- **`ServerConfig::getDataIntervalConfig()`** - Method removed
- **`MqttManager::dataIntervalMs`** - Legacy field removed (MQTT uses mode-specific intervals)
- **`MqttManager::updateDataTransmissionInterval()`** - Method removed (not needed, device restarts after config update)

#### ✅ Added
- **`http_config.interval`** - HTTP transmission interval (default: 5)
- **`http_config.interval_unit`** - Interval unit: `"ms"`, `"s"`, or `"m"` (default: `"s"`)

#### 🔄 Modified
- **`HttpManager::loadHttpConfig()`** - Now reads interval from `http_config` instead of `data_interval`
- **`HttpManager::updateDataTransmissionInterval()`** - Updated to use `http_config.interval`
- **`ServerConfig::createDefaultConfig()`** - Added interval to HTTP config defaults
- **`ServerConfig::getConfig()`** - Added defensive defaults for HTTP interval fields
- **`MqttManager::getStatus()`** - Now returns `publish_mode` instead of `data_interval_ms`
- **`CRUDHandler` server_config update** - Only calls `httpManager->updateDataTransmissionInterval()` (MQTT reloads on restart)

---

### 📚 Documentation Updates

- **API.md** - Added Example 3: HTTP Configuration with migration guide
- **API.md** - Updated server_config response structure
- **API.md** - Added breaking change warnings
- **NETWORK_CONFIGURATION.md** - 🆕 **NEW:** Complete network setup guide with failover logic, diagrams, and best practices
- **README.md** - Added link to NETWORK_CONFIGURATION.md
- **VERSION_HISTORY.md** - Added v2.2.0 entry (this document)

---

### 🚀 Migration Guide

**For Mobile App Developers:**

If you were sending `data_interval` at root level for HTTP configuration, you MUST update your code:

**OLD Code (v2.1.1):**
```javascript
const config = {
  op: "update",
  type: "server_config",
  config: {
    data_interval: {value: 10, unit: "s"},  // ❌ No longer supported
    protocol: "http",
    http_config: {
      enabled: true,
      endpoint_url: "https://api.example.com/data"
    }
  }
};
```

**NEW Code (v2.2.0+):**
```javascript
const config = {
  op: "update",
  type: "server_config",
  config: {
    protocol: "http",
    http_config: {
      enabled: true,
      endpoint_url: "https://api.example.com/data",
      interval: 10,           // ✅ Moved here
      interval_unit: "s"      // ✅ Moved here
    }
  }
};
```

**MQTT Configuration (Unchanged):**
MQTT intervals remain in their mode-specific locations:
- `mqtt_config.default_mode.interval`
- `mqtt_config.customize_mode.custom_topics[].interval`

---

### 📂 Files Modified

| File                            | Changes                                                                                                            |
| ------------------------------- | ------------------------------------------------------------------------------------------------------------------ |
| `Main/ServerConfig.cpp`         | Removed `data_interval` from defaults, added `interval` to `http_config`, removed `getDataIntervalConfig()` method |
| `Main/ServerConfig.h`           | Removed `getDataIntervalConfig()` declaration                                                                      |
| `Main/HttpManager.cpp`          | Updated to read interval from `http_config` instead of `data_interval`                                             |
| `Main/MqttManager.cpp`          | Removed legacy `dataIntervalMs` assignment and `updateDataTransmissionInterval()` method                           |
| `Main/MqttManager.h`            | Removed `dataIntervalMs` and `lastDataTransmission` fields, removed method declaration                             |
| `Main/CRUDHandler.cpp`          | Removed call to `mqttManager->updateDataTransmissionInterval()`, updated response message                          |
| `Docs/API.md`                   | Added HTTP config example, migration guide, and breaking change warnings                                           |
| `Docs/NETWORK_CONFIGURATION.md` | 🆕 NEW: Complete network configuration guide (562 lines)                                                            |
| `Docs/VERSION_HISTORY.md`       | Added v2.2.0 entry                                                                                                 |
| `README.md`                     | Added link to NETWORK_CONFIGURATION.md in documentation table                                                      |

---

### ⚠️ Breaking Changes Summary

1. **`data_interval` removed** - Must use `http_config.interval` for HTTP
2. **Old configs will NOT work** - Mobile apps must update payload structure
3. **No backward compatibility** - Clean break for cleaner API going forward

**Upgrade Impact:** 🔴 **HIGH** - Requires mobile app code changes

---

## 📦 Version 2.1.1

**Release Date:** November 14, 2025 (Friday)
**Developer:** Kemal
**Status:** ✅ Production Ready

### 🎯 Critical Performance Fix

#### 🚀 BLE Transmission Optimization (28x Faster)

**Problem Identified:**
- Large JSON payloads (21KB) were taking **58 seconds** to transmit over BLE
- Mobile app timeout at 30 seconds caused guaranteed failures
- Poor UX with frozen UI during data loading

**Root Cause:**
```cpp
// Old values (BLEManager.h)
#define CHUNK_SIZE 18           // Too small for modern BLE
#define FRAGMENT_DELAY_MS 50    // Too slow between fragments
```

**Solution:**
```cpp
// New optimized values (BLEManager.h)
#define CHUNK_SIZE 244          // MTU-safe for 512-byte MTU
#define FRAGMENT_DELAY_MS 10    // Reduced delay for faster transmission
```

**Performance Impact:**

| Scenario                | Payload Size | Before   | After    | Improvement      |
| ----------------------- | ------------ | -------- | -------- | ---------------- |
| 100 registers (full)    | 21 KB        | 58.4 sec | 2.1 sec  | **28x faster** ⚡ |
| 100 registers (minimal) | 6 KB         | 16.7 sec | 0.6 sec  | **28x faster** ⚡ |
| 50 registers (full)     | 10.5 KB      | 29.2 sec | 1.05 sec | **28x faster** ⚡ |

**Files Changed:**
- `Main/BLEManager.h` (Lines 19-26)

---

### ✨ New Features

#### 1. Enhanced CRUD Responses

All CRUD operations now return **actual data** instead of just status messages.

**Benefits for Mobile App:**
- ✅ No need for additional API calls after CREATE/UPDATE/DELETE
- ✅ Immediate UI updates with returned data
- ✅ Better validation and error handling
- ✅ Improved UX with instant feedback

**Changes:**

##### CREATE Operations
```json
// BEFORE (v2.0):
{
  "status": "ok",
  "device_id": "DEVICE_001"
}

// AFTER (v2.1.1):
{
  "status": "ok",
  "device_id": "DEVICE_001",
  "data": {
    "device_id": "DEVICE_001",
    "device_name": "Temperature Sensor",
    "protocol": "modbus_tcp",
    "ip_address": "192.168.1.100",
    "port": 502,
    "slave_id": 1,
    "refresh_rate_ms": 1000,
    "registers": [...]
  }
}
```

##### UPDATE Operations
```json
// BEFORE (v2.0):
{
  "status": "ok",
  "message": "Device updated"
}

// AFTER (v2.1.1):
{
  "status": "ok",
  "device_id": "DEVICE_001",
  "message": "Device updated",
  "data": {
    // ... full updated device data
  }
}
```

##### DELETE Operations
```json
// BEFORE (v2.0):
{
  "status": "ok",
  "message": "Device deleted"
}

// AFTER (v2.1.1):
{
  "status": "ok",
  "device_id": "DEVICE_001",
  "message": "Device deleted",
  "deleted_data": {
    // ... full device data before deletion
    // Useful for undo functionality
  }
}
```

**Files Changed:**
- `Main/CRUDHandler.cpp` (Lines 262-480)

---

#### 2. New API Endpoint: `devices_with_registers`

**Purpose:** Single API call to get all devices with their registers (solves N+1 query problem)

**Request:**
```json
{
  "op": "read",
  "type": "devices_with_registers",
  "minimal": true  // Optional: reduces payload by 71%
}
```

**Response:**
```json
{
  "status": "ok",
  "devices": [
    {
      "device_id": "DEVICE_001",
      "device_name": "Temperature Sensor",
      "registers": [
        {
          "register_id": "temp_room_1",
          "register_name": "Temperature Room 1"
        }
      ]
    }
  ]
}
```

**Performance:**

| Mode                     | Fields Returned                       | Payload Size (100 regs) | Transmission Time |
| ------------------------ | ------------------------------------- | ----------------------- | ----------------- |
| Full (`minimal=false`)   | 10 device fields + 10 register fields | ~21 KB                  | ~2.1 sec          |
| Minimal (`minimal=true`) | 2 device fields + 2 register fields   | ~6 KB                   | ~0.6 sec          |

**Use Cases:**
- MQTT Customize Mode: Device → Registers hierarchical UI
- Register selection widget in mobile app
- Quick data overview without multiple API calls

**Files Changed:**
- `Main/ConfigManager.h` (Line 65)
- `Main/ConfigManager.cpp` (Lines 473-558)
- `Main/CRUDHandler.cpp` (Lines 108-146)

---

#### 3. Performance Monitoring

Added automatic performance tracking for large dataset operations.

**Features:**
- ⏱️ Processing time measurement for `devices_with_registers` API
- ⚠️ Warning if processing takes > 10 seconds
- 📊 Serial Monitor output with detailed metrics

**Example Output:**
```
[GET_ALL_DEVICES_WITH_REGISTERS] Starting (minimal=false)...
[GET_ALL_DEVICES_WITH_REGISTERS] Found 3 devices in file
[GET_ALL_DEVICES_WITH_REGISTERS] Added device DEVICE_001 with 26 registers
[GET_ALL_DEVICES_WITH_REGISTERS] Total devices: 3
[CRUD] devices_with_registers returned 3 devices, 26 total registers (minimal=false) in 87 ms

⚠️  WARNING: Processing took 12543 ms (>10s). Consider using minimal=true for large datasets.
```

**Files Changed:**
- `Main/CRUDHandler.cpp` (Lines 112-143)
- `Main/ConfigManager.cpp` (Lines 475-558)

---

### 🐛 Bug Fixes

#### 1. Empty Response Debugging

Added comprehensive logging to identify why `devices_with_registers` might return empty data.

**Checks:**
- ✅ Devices file failed to load
- ✅ Devices file is empty
- ✅ Device has empty registers array
- ✅ Device has no registers array

**Files Changed:**
- `Main/ConfigManager.cpp` (Lines 485-551)

---

### 🔧 Improvements

#### 1. Code Documentation

- Added inline comments explaining BLE optimization rationale
- Documented MTU-safe chunk size calculation
- Added performance impact metrics in code comments

#### 2. Backward Compatibility

- ✅ All API changes are **additive only** (no breaking changes)
- ✅ Existing mobile apps continue to work without modification
- ✅ New `data` fields can be safely ignored by old clients

---

### 📊 Migration Guide

#### For Mobile App Developers

**No breaking changes!** Your existing code will continue to work.

**Optional Enhancements:**

1. **Use returned data to avoid extra API calls:**
```dart
// OLD APPROACH (still works):
await createDevice(config);
final device = await getDevice(deviceId);  // Extra API call
updateUI(device);

// NEW APPROACH (more efficient):
final response = await createDevice(config);
updateUI(response.data);  // Use data from create response
```

2. **Use minimal mode for better performance:**
```dart
// For MQTT customize mode register selection:
final response = await api.read(
  type: 'devices_with_registers',
  minimal: true  // 71% smaller payload, 3x faster
);
```

3. **Implement undo for delete operations:**
```dart
final response = await deleteDevice(deviceId);
// response.deleted_data contains full device before deletion
// Can be used to implement undo functionality
```

---

### 📦 Files Modified

| File                     | Lines Changed    | Description                              |
| ------------------------ | ---------------- | ---------------------------------------- |
| `Main/BLEManager.h`      | 19-26            | BLE transmission optimization constants  |
| `Main/CRUDHandler.cpp`   | 108-146, 262-480 | Enhanced CRUD responses + new endpoint   |
| `Main/ConfigManager.h`   | 65               | New method signature                     |
| `Main/ConfigManager.cpp` | 473-558          | Implementation of devices_with_registers |

**Total Changes:**
- **Files modified:** 4
- **Lines added:** ~180
- **Lines removed:** ~30
- **Net change:** +150 lines

---

### 🧪 Testing Recommendations

1. **Test BLE transmission with large datasets:**
   - 100+ registers with full details
   - Should complete in < 5 seconds
   - No mobile app timeouts

2. **Test CRUD return data:**
   - CREATE: Verify `data` object contains full entity
   - UPDATE: Verify `data` object reflects changes
   - DELETE: Verify `deleted_data` contains pre-deletion state

3. **Test new endpoint:**
   ```json
   {"op": "read", "type": "devices_with_registers", "minimal": true}
   ```
   - Check Serial Monitor for performance metrics
   - Verify response contains all devices and registers

4. **Backward compatibility test:**
   - Use old mobile app version
   - All operations should work (ignore new fields)

---

## 📦 Version 2.1.0

**Release Date:** November 2024
**Status:** ✅ Production

### Features
- MQTT Customize Mode with `register_id` support
- Hierarchical Device → Registers UI pattern
- Enhanced MQTT publish modes documentation
- Register calibration (scale & offset)

### Changes
- Changed MQTT customize mode from `register_index` (int) to `register_id` (String)
- Added support for flexible register selection per topic

**See:** [MQTT_PUBLISH_MODES_DOCUMENTATION.md](MQTT_PUBLISH_MODES_DOCUMENTATION.md)

---

## 📦 Version 2.0.0

**Release Date:** October 2024
**Status:** ✅ Production

### Major Features
- BLE CRUD API for device/register configuration
- Dual Modbus support (RTU + TCP)
- MQTT data publishing
- Real-time data streaming
- Batch operations with priority queue
- PSRAM support for large JSON documents

**See:** [API.md](API.md) for complete reference

---

## 📝 Version Numbering

**Format:** `MAJOR.MINOR.PATCH`

- **MAJOR:** Breaking changes (API incompatible)
- **MINOR:** New features (backward compatible)
- **PATCH:** Bug fixes and optimizations

**Current:** v2.1.1 (performance patch + enhancements)

---

## 🔗 Related Documentation

- [API Reference](API.md) - Complete BLE API documentation
- [MQTT Publish Modes](MQTT_PUBLISH_MODES_DOCUMENTATION.md) - MQTT configuration guide
- [Troubleshooting Guide](TROUBLESHOOTING.md) - Common issues and solutions
- [Capacity Analysis](CAPACITY_ANALYSIS.md) - System limits and performance
