# Mobile App Optimization Guide for Firmware Compatibility

**Version:** 1.0 | **Date:** December 12, 2025 | **Firmware Version:** 2.5.35+

---

## Overview

This document outlines all optimization points for the Suriota Mobile App to ensure full compatibility with the latest firmware features and optimal performance.

### Priority Legend
- **P0** - Critical, must fix immediately
- **P1** - High priority, should fix soon
- **P2** - Medium priority, plan for next release
- **P3** - Low priority, nice to have

---

## Table of Contents

1. [P0: MTU Negotiation (25x Speed Boost)](#1-p0-mtu-negotiation-25x-speed-boost)
2. [P1: BLE Device Name Filter Update](#2-p1-ble-device-name-filter-update)
3. [P1: Factory Reset Default Values Sync](#3-p1-factory-reset-default-values-sync)
4. [P2: OTA Firmware Update UI](#4-p2-ota-firmware-update-ui)
5. [P2: Gateway Info UI](#5-p2-gateway-info-ui)
6. [P3: Device Enable/Disable UI](#6-p3-device-enabledisable-ui)
7. [P3: Production Mode Toggle](#7-p3-production-mode-toggle)
8. [P3: Timeout Optimization](#8-p3-timeout-optimization)
9. [P3: Streaming Delay Optimization](#9-p3-streaming-delay-optimization)

---

## 1. P0: MTU Negotiation (25x Speed Boost)

**Status:** Separate document available
**Document:** `BLE_MTU_OPTIMIZATION.md`

### Summary
Current BLE transfer is very slow (18 bytes/chunk, 100ms delay). Implementing MTU negotiation can achieve ~25x faster transfers.

| Metric | Before | After |
|--------|--------|-------|
| Chunk Size | 18 bytes | 200 bytes |
| Chunk Delay | 100ms | 15ms |
| 1KB Transfer | ~7.7 sec | ~0.3 sec |

**See `BLE_MTU_OPTIMIZATION.md` for complete implementation guide.**

---

## 2. P1: BLE Device Name Filter Update

### Problem

Firmware v2.5.32 changed BLE advertising name format:
- **Old format:** `SURIOTA-XXXXXX`
- **New format:** `MGate-1210(P)-XXXX` or `MGate-1210-XXXX`

Current mobile app filter only matches `mgate`:
```dart
// ble_controller.dart line 54
static const String deviceNameFilter = 'mgate';
```

This works for new format but would miss old devices still running older firmware.

### Solution

Update to support both formats:

**File:** `lib/core/controllers/ble_controller.dart`

```dart
// BEFORE (line 54):
static const String deviceNameFilter = 'mgate';

// AFTER:
/// Device name filters for BLE scanning
/// Supports both old (SURIOTA-) and new (MGate-1210) naming conventions
static const List<String> deviceNameFilters = ['mgate', 'suriota'];
```

Update the filter check method:

```dart
// BEFORE (line 341-342):
bool _matchesDeviceFilter(String deviceName) =>
    deviceName.contains(deviceNameFilter);

// AFTER:
bool _matchesDeviceFilter(String deviceName) {
  final lowerName = deviceName.toLowerCase();
  return deviceNameFilters.any((filter) => lowerName.contains(filter));
}
```

### Testing
- Scan should find devices with names like:
  - `MGate-1210(P)-A716`
  - `MGate-1210-C726`
  - `SURIOTA-A1B2C3` (legacy)

---

## 3. P1: Factory Reset Default Values Sync

### Problem

Mobile app form defaults don't match firmware factory reset defaults after v2.5.35 fix.

**Mobile App defaults (`form_config_server_screen.dart` lines 44-49):**
```dart
String communicationSelected = 'ETH';      // Should be 'WIFI'
String isWifiEnabled = 'true';             // Should be 'false'
String isEthernetEnabled = 'true';         // Should be 'false'
String isEnabledMqtt = 'true';             // Should be 'false'
```

**Firmware v2.5.35 factory reset defaults:**
```cpp
comm["mode"] = "WIFI";
wifi["enabled"] = false;
ethernet["enabled"] = false;
mqtt["enabled"] = false;
mqtt["client_id"] = "SRT_MGate1210_XXXX";  // Unique per device
```

### Solution

**File:** `lib/presentation/pages/devices/server_config/form_config_server_screen.dart`

```dart
// BEFORE (lines 44-49):
String communicationSelected = 'ETH';
String isWifiEnabled = 'true';
String isEthernetEnabled = 'true';
String isUseDhcp = 'true';
String isEnabledMqtt = 'true';
String isEnabledHttp = 'false';

// AFTER:
// Match firmware v2.5.35 factory reset defaults
String communicationSelected = 'WIFI';    // Changed from ETH
String isWifiEnabled = 'false';           // Changed from true
String isEthernetEnabled = 'false';       // Changed from true
String isUseDhcp = 'true';                // Unchanged
String isEnabledMqtt = 'false';           // Changed from true
String isEnabledHttp = 'false';           // Unchanged
```

### Impact
- New device setup will show correct defaults
- User won't be confused by mismatched initial values
- Reduces support tickets about "wrong defaults"

---

## 4. P2: OTA Firmware Update UI

### Problem

`update_firmware_screen.dart` only shows "Coming Soon" placeholder, but firmware fully supports OTA.

### Firmware OTA Commands Available

```dart
// 1. Check for updates
{"op": "control", "type": "check_update"}
// Response: {update_available, current_version, target_version, manifest}

// 2. Start OTA download
{"op": "control", "type": "start_update"}
// Or with custom URL:
{"op": "control", "type": "start_update", "url": "https://..."}

// 3. Get OTA status/progress
{"op": "control", "type": "ota_status"}
// Response: {state, state_name, progress, bytes_downloaded, total_bytes}

// 4. Apply downloaded firmware (triggers reboot)
{"op": "control", "type": "apply_update"}

// 5. Abort ongoing OTA
{"op": "control", "type": "abort_update"}

// 6. Rollback firmware
{"op": "control", "type": "rollback", "target": "previous"}
{"op": "control", "type": "rollback", "target": "factory"}

// 7. Get OTA configuration
{"op": "control", "type": "get_ota_config"}

// 8. Set GitHub repository for OTA
{"op": "control", "type": "set_github_repo",
 "owner": "GifariKemal", "repo": "GatewaySuriotaOTA", "branch": "main"}

// 9. Set GitHub token (for private repos)
{"op": "control", "type": "set_github_token", "token": "ghp_xxx..."}
```

### OTA States

| State | Value | Description |
|-------|-------|-------------|
| IDLE | 0 | No OTA in progress |
| CHECKING | 1 | Checking for updates |
| DOWNLOADING | 2 | Downloading firmware |
| VALIDATING | 3 | Verifying signature |
| APPLYING | 4 | Writing to flash |
| REBOOTING | 5 | About to reboot |
| ERROR | 6 | Error occurred |

### Suggested UI Flow

```
┌─────────────────────────────────────┐
│         Firmware Update             │
├─────────────────────────────────────┤
│ Current Version: 2.5.34             │
│ Latest Version:  2.5.35             │
│                                     │
│ [Check for Updates]                 │
│                                     │
│ ┌─────────────────────────────────┐ │
│ │ Update Available!               │ │
│ │ Version: 2.5.35                 │ │
│ │ Size: 1.94 MB                   │ │
│ │                                 │ │
│ │ Changelog:                      │ │
│ │ - OTA apply fix                 │ │
│ │ - Factory reset defaults fix    │ │
│ └─────────────────────────────────┘ │
│                                     │
│ [Download Update]                   │
│                                     │
│ ████████████░░░░░░░░ 65%           │
│ Downloaded: 1.26 MB / 1.94 MB       │
│                                     │
│ [Cancel]                            │
│                                     │
│ ─────────────────────────────────── │
│ Advanced Options:                   │
│ [Rollback to Previous]              │
│ [Rollback to Factory]               │
│ [Configure OTA Source]              │
└─────────────────────────────────────┘
```

### Implementation Skeleton

**File:** `lib/presentation/pages/devices/status/update_firmware_screen.dart`

```dart
class UpdateFirmwareScreen extends StatefulWidget {
  final DeviceModel model;
  const UpdateFirmwareScreen({super.key, required this.model});

  @override
  State<UpdateFirmwareScreen> createState() => _UpdateFirmwareScreenState();
}

class _UpdateFirmwareScreenState extends State<UpdateFirmwareScreen> {
  final BleController bleController = Get.find<BleController>();

  // OTA State
  bool isChecking = false;
  bool isDownloading = false;
  bool updateAvailable = false;

  String currentVersion = '';
  String targetVersion = '';
  int progress = 0;
  int bytesDownloaded = 0;
  int totalBytes = 0;
  String otaState = 'IDLE';
  String? errorMessage;

  Timer? _statusPoller;

  @override
  void initState() {
    super.initState();
    _checkCurrentVersion();
  }

  @override
  void dispose() {
    _statusPoller?.cancel();
    super.dispose();
  }

  Future<void> _checkCurrentVersion() async {
    final response = await bleController.sendCommand({
      "op": "control",
      "type": "ota_status",
    });

    if (response.status == 'ok') {
      setState(() {
        currentVersion = response.config?['current_version'] ?? '';
        otaState = response.config?['state_name'] ?? 'IDLE';
      });
    }
  }

  Future<void> _checkForUpdates() async {
    setState(() => isChecking = true);

    final response = await bleController.sendCommand({
      "op": "control",
      "type": "check_update",
    });

    setState(() {
      isChecking = false;
      if (response.status == 'ok') {
        updateAvailable = response.config?['update_available'] ?? false;
        currentVersion = response.config?['current_version'] ?? '';
        targetVersion = response.config?['target_version'] ?? '';
      }
    });
  }

  Future<void> _startUpdate() async {
    final response = await bleController.sendCommand({
      "op": "control",
      "type": "start_update",
    });

    if (response.status == 'ok') {
      setState(() => isDownloading = true);
      _startStatusPolling();
    } else {
      setState(() => errorMessage = response.message);
    }
  }

  void _startStatusPolling() {
    _statusPoller = Timer.periodic(Duration(seconds: 2), (_) async {
      final response = await bleController.sendCommand({
        "op": "control",
        "type": "ota_status",
      });

      if (response.status == 'ok') {
        setState(() {
          otaState = response.config?['state_name'] ?? 'IDLE';
          progress = response.config?['progress'] ?? 0;
          bytesDownloaded = response.config?['bytes_downloaded'] ?? 0;
          totalBytes = response.config?['total_bytes'] ?? 0;
        });

        // Stop polling when done or error
        if (otaState == 'VALIDATING' || otaState == 'ERROR' || otaState == 'IDLE') {
          _statusPoller?.cancel();
          setState(() => isDownloading = false);
        }
      }
    });
  }

  Future<void> _applyUpdate() async {
    // Show confirmation dialog first
    final confirmed = await _showConfirmDialog(
      'Apply Update',
      'Device will reboot to apply the update. Continue?',
    );

    if (confirmed) {
      await bleController.sendCommand({
        "op": "control",
        "type": "apply_update",
      });
      // Device will reboot and disconnect
    }
  }

  Future<void> _abortUpdate() async {
    await bleController.sendCommand({
      "op": "control",
      "type": "abort_update",
    });
    _statusPoller?.cancel();
    setState(() {
      isDownloading = false;
      otaState = 'IDLE';
    });
  }

  // ... Build UI based on state ...
}
```

---

## 5. P2: Gateway Info UI

### Problem

Firmware v2.5.31 added gateway identity management, but mobile app has no UI for it.

### Firmware Commands

```dart
// Get gateway info
{"op": "control", "type": "get_gateway_info"}
// Response:
{
  "status": "ok",
  "gateway_id": "A1B2C3D4E5F6",
  "friendly_name": "Production Gateway 1",
  "location": "Building A, Floor 2",
  "ble_name": "MGate-1210(P)-A716",
  "serial_number": "SRT-MGATE1210P-20251212-A1B2C3",
  "firmware_version": "2.5.35",
  "hardware_version": "1.0"
}

// Set friendly name
{"op": "control", "type": "set_friendly_name", "name": "My Gateway"}

// Set location
{"op": "control", "type": "set_location", "location": "Building A"}
```

### Suggested UI Location

Add to **Settings Device Screen** or create new **Gateway Info Screen**:

```
┌─────────────────────────────────────┐
│         Gateway Information         │
├─────────────────────────────────────┤
│ Gateway ID:      A1B2C3D4E5F6       │
│ BLE Name:        MGate-1210(P)-A716 │
│ Serial Number:   SRT-MGATE1210P-... │
│                                     │
│ Firmware:        2.5.35             │
│ Hardware:        1.0                │
│                                     │
│ ─────────────────────────────────── │
│ Friendly Name:                      │
│ ┌─────────────────────────────────┐ │
│ │ Production Gateway 1        [✏️]│ │
│ └─────────────────────────────────┘ │
│                                     │
│ Location:                           │
│ ┌─────────────────────────────────┐ │
│ │ Building A, Floor 2         [✏️]│ │
│ └─────────────────────────────────┘ │
│                                     │
│ [Save Changes]                      │
└─────────────────────────────────────┘
```

---

## 6. P3: Device Enable/Disable UI

### Problem

Firmware supports enabling/disabling individual Modbus devices, but mobile app lacks UI for this.

### Firmware Commands

```dart
// Enable device
{"op": "control", "type": "enable_device", "device_id": "D7A3F2"}

// Disable device
{"op": "control", "type": "disable_device", "device_id": "D7A3F2"}

// Get device status (includes enable/disable state)
{"op": "read", "type": "device_status", "device_id": "D7A3F2"}
// Response includes:
{
  "device_id": "D7A3F2",
  "enabled": true,
  "health": {
    "success_rate": 98.5,
    "consecutive_failures": 0,
    "total_reads": 1000,
    "successful_reads": 985
  }
}

// Get all devices status
{"op": "read", "type": "all_devices_status"}
```

### Suggested UI

Add toggle to device list or detail screen:

```
┌─────────────────────────────────────┐
│ Device: Power Meter                 │
│ ID: D7A3F2 | Protocol: TCP          │
├─────────────────────────────────────┤
│ Status:    [████████░░] 98.5%       │
│ Enabled:   [ON ══════ OFF]  ← Toggle│
│                                     │
│ Health Metrics:                     │
│ • Total Reads: 1,000                │
│ • Success: 985 | Failed: 15         │
│ • Avg Response: 45ms                │
└─────────────────────────────────────┘
```

---

## 7. P3: Production Mode Toggle (Developer Only)

### Problem

Firmware supports production/development mode toggle via BLE, but no UI exists.

### IMPORTANT: Password Protection Required

This feature should be **hidden from regular users** and only accessible by developers/technicians.

**Implementation Requirement:**
- This menu should be placed in a **hidden/developer section**
- Access requires a **developer password** before showing the toggle
- Suggested password: Use a fixed developer PIN or company-specific code
- Do NOT expose this to end users - it's for factory/service use only

### Firmware Commands

```dart
// Read current mode
{"op": "read", "type": "production_mode"}
// Response: {"production_mode": true, "log_level": "ERROR"}

// Set production mode
{"op": "update", "type": "production_mode", "config": {"enabled": true}}
// Response: {"status": "ok", "message": "Production mode enabled", "restart_required": true}
```

### Mode Differences

| Aspect | Development Mode | Production Mode |
|--------|------------------|-----------------|
| Log Level | INFO/DEBUG | ERROR only |
| BLE Access | Always on | Button-activated |
| Serial Output | Verbose | Minimal JSON |
| Memory Usage | Higher | Optimized |

### Suggested UI Flow

**Step 1: Hidden Access Point**
- Add hidden tap gesture (e.g., tap version number 5 times in About screen)
- Or add hidden menu item only visible in debug builds

**Step 2: Password Dialog**
```
┌─────────────────────────────────────┐
│         Developer Access            │
├─────────────────────────────────────┤
│                                     │
│ Enter developer password:           │
│ ┌─────────────────────────────────┐ │
│ │ ••••••••                        │ │
│ └─────────────────────────────────┘ │
│                                     │
│ [Cancel]              [Authenticate]│
└─────────────────────────────────────┘
```

**Step 3: Developer Menu (after authentication)**
```
┌─────────────────────────────────────┐
│      Developer Settings             │
├─────────────────────────────────────┤
│ Production Mode: [ON ══════ OFF]    │
│                                     │
│ ⚠️ Warning:                         │
│ • Changes device behavior           │
│ • Reduces logging to errors only    │
│ • BLE requires button press         │
│ • Device will restart after change  │
│                                     │
│ Current: Development Mode           │
│ Log Level: INFO                     │
└─────────────────────────────────────┘
```

### Implementation Example

```dart
// Password check before showing developer menu
class DeveloperAccessDialog extends StatelessWidget {
  static const String developerPassword = 'SURIOTA2025';  // Or from secure storage

  final TextEditingController _passwordController = TextEditingController();

  Future<bool> authenticate(BuildContext context) async {
    return await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: Text('Developer Access'),
        content: TextField(
          controller: _passwordController,
          obscureText: true,
          decoration: InputDecoration(
            labelText: 'Enter developer password',
          ),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context, false),
            child: Text('Cancel'),
          ),
          TextButton(
            onPressed: () {
              final isValid = _passwordController.text == developerPassword;
              Navigator.pop(context, isValid);
              if (!isValid) {
                ScaffoldMessenger.of(context).showSnackBar(
                  SnackBar(content: Text('Invalid password')),
                );
              }
            },
            child: Text('Authenticate'),
          ),
        ],
      ),
    ) ?? false;
  }
}

// Hidden access: Tap version 5 times
int _tapCount = 0;
Timer? _tapResetTimer;

void _onVersionTap(BuildContext context) {
  _tapCount++;
  _tapResetTimer?.cancel();
  _tapResetTimer = Timer(Duration(seconds: 2), () => _tapCount = 0);

  if (_tapCount >= 5) {
    _tapCount = 0;
    _showDeveloperAccess(context);
  }
}
```

---

## 8. P3: Timeout Optimization

### Problem

After MTU optimization, BLE transfers will be much faster, but timeouts are still set for slow transfers.

### Current Values (`ble_controller.dart`)

```dart
static const Duration timeoutStopCommand = Duration(seconds: 5);
static const Duration timeoutDevicesSummary = Duration(seconds: 60);
static const Duration timeoutSingleDevice = Duration(seconds: 90);
static const Duration timeoutPaginatedRequest = Duration(seconds: 360);    // 6 min!
static const Duration timeoutWriteOperation = Duration(seconds: 45);
static const Duration timeoutDefault = Duration(seconds: 90);
static const Duration timeoutLargeDataMinimal = Duration(seconds: 900);    // 15 min!
static const Duration timeoutLargeDataFull = Duration(seconds: 900);       // 15 min!
```

### Recommended Values (Post-MTU Optimization)

```dart
static const Duration timeoutStopCommand = Duration(seconds: 5);           // Keep
static const Duration timeoutDevicesSummary = Duration(seconds: 30);       // Reduced from 60
static const Duration timeoutSingleDevice = Duration(seconds: 30);         // Reduced from 90
static const Duration timeoutPaginatedRequest = Duration(seconds: 60);     // Reduced from 360
static const Duration timeoutWriteOperation = Duration(seconds: 30);       // Reduced from 45
static const Duration timeoutDefault = Duration(seconds: 45);              // Reduced from 90
static const Duration timeoutLargeDataMinimal = Duration(seconds: 120);    // Reduced from 900
static const Duration timeoutLargeDataFull = Duration(seconds: 180);       // Reduced from 900
```

### Impact
- Better UX - errors shown faster instead of waiting 15 minutes
- More responsive app behavior
- User knows sooner if something went wrong

---

## 9. P3: Streaming Delay Optimization

### Problem

Streaming methods use hardcoded delays that could be optimized after MTU negotiation.

### Current Values

```dart
// In startDataStream and startEnhancedStreaming
await Future.delayed(const Duration(milliseconds: 50));
```

### Recommended

Use the dynamic delay from MTU negotiation:

```dart
// After MTU optimization implementation
await Future.delayed(_currentChunkDelay);  // 15ms with high MTU, 50ms fallback
```

### Files to Update
- `startDataStream()` - around line 2254
- `startEnhancedStreaming()` - around line 2593
- `_sendCommandManually()` - around line 2313

---

## Implementation Summary

### Quick Wins (< 1 hour each)

| Item | File | Lines to Change |
|------|------|-----------------|
| Device name filter | `ble_controller.dart` | ~5 lines |
| Factory reset defaults | `form_config_server_screen.dart` | ~5 lines |
| Timeout reduction | `ble_controller.dart` | ~8 lines |
| Streaming delay | `ble_controller.dart` | ~3 locations |

### Medium Effort (1-4 hours each)

| Item | Effort |
|------|--------|
| MTU Negotiation | 2-3 hours (see separate guide) |
| Gateway Info UI | 2-3 hours |
| Production Mode Toggle | 1-2 hours |

### Major Features (1+ days)

| Item | Effort |
|------|--------|
| OTA Firmware Update UI | 2-3 days |
| Device Enable/Disable UI | 1-2 days |

---

## Testing Checklist

After implementing optimizations:

- [ ] **BLE Scan** - Finds both old (SURIOTA-) and new (MGate-) devices
- [ ] **MTU** - Debug log shows negotiated MTU > 100 bytes
- [ ] **Transfer Speed** - Large config backup completes in < 10 seconds
- [ ] **Factory Reset** - Form shows WiFi mode, all disabled by default
- [ ] **Timeouts** - Errors appear within 30-60 seconds, not 15 minutes
- [ ] **OTA** (if implemented) - Can check, download, and apply updates
- [ ] **Gateway Info** (if implemented) - Can view and edit name/location

---

## Firmware Compatibility Matrix

| Mobile App Feature | Min Firmware | Status |
|--------------------|--------------|--------|
| Basic CRUD | 2.0.0+ | ✅ Supported |
| MQTT Modes | 2.2.0+ | ✅ Supported |
| Backup/Restore | 2.3.0+ | ✅ Supported |
| Device Enable/Disable | 2.3.0+ | ⚠️ No UI |
| Gateway Identity | 2.5.31+ | ⚠️ No UI |
| OTA via BLE | 2.5.0+ | ⚠️ No UI |
| New BLE Name Format | 2.5.32+ | ⚠️ Filter needs update |
| Factory Reset Defaults | 2.5.35+ | ⚠️ Defaults mismatch |

---

## Contact & Resources

**Firmware Documentation:**
- `Documentation/API_Reference/API.md` - Complete BLE CRUD API
- `Documentation/API_Reference/BLE_DEVICE_CONTROL.md` - Enable/Disable API
- `Documentation/Technical_Guides/BLE_MTU_OPTIMIZATION.md` - MTU Guide

**Firmware Source:**
- `Main/CRUDHandler.cpp` - Command processing
- `Main/BLEManager.cpp` - BLE handling
- `Main/OTACrudBridge.cpp` - OTA commands
- `Main/GatewayConfig.cpp` - Gateway identity

---

**Document prepared by Claude Code** | December 12, 2025
