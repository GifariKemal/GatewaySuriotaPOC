# Version History

**SRT-MGATE-1210 Modbus IIoT Gateway**
**Firmware Release Notes**

**Developer:** SURIOTA R&D Team
**Timezone:** WIB (GMT+7)

---

## Version 1.0.3 (Config Transfer Progress)

**Release Date:** December 27, 2025 (Friday)
**Status:** Production (⚠️ BREAKING CHANGE - Mobile App Update Required)

### Feature: Config Transfer Progress Notifications - ACTIVATED

**Background:** Unlike OTA which has real-time progress notifications, config backup/restore operations had no progress reporting to the mobile app.

**Status:** ✅ Progress notifications now ACTIVE!

### ⚠️ BREAKING CHANGE

Mobile app **MUST** be updated to handle new notification types before using this firmware version. Without update, backup/restore operations will have **corrupted JSON responses**.

### New Notification Types

**1. config_download_progress** (during backup/export):
```json
{"type":"config_download_progress","percent":45,"bytes_sent":5000,"total_bytes":11000}
```
- Sent every 10% during large response transmission (>5KB)
- Mobile app must filter this from response buffer

**2. config_restore_progress** (during restore/import):
```json
{"type":"config_restore_progress","step":"devices","current":1,"total":3,"percent":33}
```
- Sent at each restore step: `devices` → `server_config` → `logging_config`
- Mobile app must filter this from response buffer

**3. config_upload_progress** (NOT sent by firmware):
```json
{"type":"config_upload_progress","percent":60,"bytes_received":6000,"total_expected":10000}
```
- Function exists but NOT called - mobile app should track upload progress locally
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

- `Main/BLEManager.cpp` - Activated `sendConfigDownloadProgress()` in `sendFragmented()`
- `Main/CRUDHandler.cpp` - Added `sendConfigRestoreProgress()` calls in restore handler
- `Main/ProductConfig.h` - Version bump to 1.0.3

---

## Version 1.0.2 (Standardized Error Responses)

**Release Date:** December 28, 2025 (Saturday)
**Status:** Production

### Feature: Standardized Error Response Format

**Background:** BLE CRUD responses previously used inconsistent error formats with string-based error codes. Mobile apps couldn't programmatically handle errors reliably.

**Solution:** Implemented `ErrorResponseHelper` utility that creates consistent error responses with:
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

| Domain | Range | Examples |
|--------|-------|----------|
| NETWORK | 0-99 | WiFi/Ethernet connectivity |
| MQTT | 100-199 | Broker communication |
| BLE | 200-299 | Bluetooth Low Energy |
| MODBUS | 300-399 | RTU/TCP devices |
| MEMORY | 400-499 | PSRAM allocation |
| CONFIG | 500-599 | Configuration/Storage |
| SYSTEM | 600-699 | System health |

### Files Added

- `Main/ErrorResponseHelper.h` - Helper functions for standardized error responses

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

Functions implemented but not activated in v1.0.2. See v1.0.3 for activated version.

**Functions Added:**
```cpp
void sendConfigDownloadProgress(uint8_t percent, size_t bytesSent, size_t totalBytes);
void sendConfigUploadProgress(uint8_t percent, size_t bytesReceived, size_t totalExpected);
void sendConfigRestoreProgress(const String& step, uint8_t currentStep, uint8_t totalSteps);
```

---

## Version 1.0.1 (Patch Release)

**Release Date:** December 27, 2025 (Friday)
**Status:** Production Patch

### Critical Bug Fix

**Issue:** Modbus TCP/RTU config changes (IP address, slave ID, etc.) took ~2 minutes to take effect.

**Symptoms:**
- Config change notification received immediately
- Old config still used for polling for ~2 minutes
- Logs showed "ERROR" connecting to old IP until device refresh

**Root Cause:**
- `configChangePending` flag only checked BETWEEN devices (line 324)
- NOT checked BETWEEN registers during device polling
- With 45 registers × 3 second timeout = **135 seconds delay** before config refresh

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

**Release Date:** December 27, 2025 (Friday)
**Status:** Production Ready

### Overview

First official production release of the SRT-MGATE-1210 Industrial IoT Gateway firmware. This release consolidates all development work from the Alpha/Beta phases (v0.x.x) into a stable, production-ready firmware.

---

### Core Features

#### 1. Modbus Protocol Support
- **Modbus RTU** - Dual RS485 ports with configurable baud rate, parity, stop bits
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

| Component | Specification |
|-----------|---------------|
| MCU | ESP32-S3 (240MHz dual-core) |
| SRAM | 512KB |
| PSRAM | 8MB |
| Flash | 16MB |
| RS485 | 2 ports (MAX485) |
| Ethernet | W5500 |
| BLE | BLE 5.0 |

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

| Category | Score | Details |
|----------|-------|---------|
| Logging System | 97% | Two-tier with 15+ module macros |
| Thread Safety | 96% | Mutex-protected shared resources |
| Memory Management | 95% | PSRAM-first with DRAM fallback |
| Error Handling | 93% | Unified codes, 7 domains |
| Testing Coverage | 70% | Integration tests, simulators |

**Overall Score:** 91/100 (Production Ready)

---

### Related Documentation

- [API Reference](../API_Reference/API.md) - Complete BLE API documentation
- [Modbus Data Types](../Technical_Guides/MODBUS_DATATYPES.md) - 40+ supported types
- [Protocol Guide](../Technical_Guides/PROTOCOL.md) - BLE/Modbus protocol specs
- [Troubleshooting](../Technical_Guides/TROUBLESHOOTING.md) - Common issues and solutions

---

**Made by SURIOTA R&D Team** | Industrial IoT Solutions
**Contact:** support@suriota.com | www.suriota.com
