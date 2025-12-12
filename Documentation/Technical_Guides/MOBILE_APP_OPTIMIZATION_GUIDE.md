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
4. [P1: Loading Progress Percentage](#4-p1-loading-progress-percentage)
5. [P2: OTA Firmware Update UI](#5-p2-ota-firmware-update-ui)
6. [P2: Gateway Info UI](#6-p2-gateway-info-ui)
7. [P3: Device Enable/Disable UI](#7-p3-device-enabledisable-ui)
8. [P3: Production Mode Toggle](#8-p3-production-mode-toggle)
9. [P3: Timeout Optimization](#9-p3-timeout-optimization)
10. [P3: Streaming Delay Optimization](#10-p3-streaming-delay-optimization)

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

## 4. P1: Loading Progress Percentage

### Problem

Currently, loading operations show generic "Loading..." text without progress indication. Users don't know:
- How much data has been transferred
- How long to wait
- If the app is frozen or working

### Why This Matters

| Without Progress | With Progress |
|------------------|---------------|
| "Loading..." (user waits blindly) | "Loading... 45%" (user knows status) |
| User thinks app is frozen | User sees active progress |
| No idea how long to wait | Can estimate remaining time |
| Frustrating UX | Professional UX |

### Progress Calculation Formula

BLE transfer progress can be calculated from:

```dart
// Send Progress (App → Device)
sendProgress = bytesSent / totalBytesToSend * 100

// Receive Progress (Device → App)
receiveProgress = bytesReceived / expectedTotalBytes * 100

// Combined Progress (50% send, 50% receive)
overallProgress = (sendProgress * 0.3) + (receiveProgress * 0.7)
```

### Implementation: Progress Stages

```dart
enum CommandStage {
  preparing,      // 0-10%
  sending,        // 10-30%
  waiting,        // 30-40%
  receiving,      // 40-90%
  processing,     // 90-100%
}

class CommandProgress {
  double progress = 0.0;
  String stage = '';

  void updateStage(CommandStage stage, {double? customProgress}) {
    switch (stage) {
      case CommandStage.preparing:
        progress = customProgress ?? 5.0;
        this.stage = 'Preparing...';
        break;
      case CommandStage.sending:
        progress = customProgress ?? 20.0;
        this.stage = 'Sending command...';
        break;
      case CommandStage.waiting:
        progress = customProgress ?? 35.0;
        this.stage = 'Waiting for response...';
        break;
      case CommandStage.receiving:
        progress = customProgress ?? 70.0;
        this.stage = 'Receiving data...';
        break;
      case CommandStage.processing:
        progress = customProgress ?? 95.0;
        this.stage = 'Processing...';
        break;
    }
  }
}
```

### Implementation: Chunked Send Progress

```dart
Future<void> sendCommandWithProgress({
  required Map<String, dynamic> command,
  required Function(double progress, String status) onProgress,
}) async {
  final jsonStr = jsonEncode(command);
  final totalBytes = jsonStr.length;
  final chunkSize = _currentChunkSize;  // From MTU negotiation
  final totalChunks = (totalBytes / chunkSize).ceil();

  onProgress(5.0, 'Preparing command...');

  int sentBytes = 0;
  int currentChunk = 0;

  for (int i = 0; i < totalBytes; i += chunkSize) {
    final chunk = jsonStr.substring(
      i,
      (i + chunkSize > totalBytes) ? totalBytes : i + chunkSize
    );

    await commandChar!.write(utf8.encode(chunk));

    sentBytes += chunk.length;
    currentChunk++;

    // Calculate send progress (10% - 40% of total)
    final sendProgress = 10 + (sentBytes / totalBytes * 30);
    onProgress(
      sendProgress,
      'Sending... ${currentChunk}/${totalChunks} chunks (${sentBytes}/${totalBytes} bytes)'
    );

    await Future.delayed(_currentChunkDelay);
  }

  // Send END marker
  await commandChar!.write(utf8.encode('<END>'));
  onProgress(40.0, 'Command sent, waiting for response...');
}
```

### Implementation: Chunked Receive Progress

```dart
void setupReceiveProgressTracking({
  required int? expectedSize,
  required Function(double progress, String status) onProgress,
}) {
  int receivedBytes = 0;
  final List<String> chunks = [];

  responseSubscription = responseChar!.lastValueStream.listen((data) {
    final chunk = utf8.decode(data, allowMalformed: true);

    if (chunk == '<END>') {
      onProgress(95.0, 'Processing response...');
      final fullResponse = chunks.join('');
      onProgress(100.0, 'Complete!');
    } else {
      chunks.add(chunk);
      receivedBytes += chunk.length;

      // Calculate receive progress (40% - 90% of total)
      double receiveProgress;
      if (expectedSize != null && expectedSize > 0) {
        receiveProgress = 40 + (receivedBytes / expectedSize * 50);
        receiveProgress = receiveProgress.clamp(40.0, 90.0);
      } else {
        // Unknown size: logarithmic estimate
        receiveProgress = 40 + (50 * (1 - 1 / (1 + receivedBytes / 1000)));
        receiveProgress = receiveProgress.clamp(40.0, 85.0);
      }

      onProgress(
        receiveProgress,
        'Receiving... ${_formatBytes(receivedBytes)}${expectedSize != null ? ' / ${_formatBytes(expectedSize)}' : ''}'
      );
    }
  });
}

String _formatBytes(int bytes) {
  if (bytes < 1024) return '${bytes} B';
  if (bytes < 1024 * 1024) return '${(bytes / 1024).toStringAsFixed(1)} KB';
  return '${(bytes / 1024 / 1024).toStringAsFixed(2)} MB';
}
```

### Implementation: Multi-Step Operations

```dart
Future<void> fetchAllDevicesWithProgress({
  required Function(double progress, String status) onProgress,
}) async {
  // Step 1: Get summary (0-20%)
  onProgress(5.0, 'Fetching device summary...');
  final summary = await sendCommand({'op': 'read', 'type': 'devices_summary'});
  onProgress(20.0, 'Got ${summary.devices.length} devices');

  final totalDevices = summary.devices.length;
  final List<Map<String, dynamic>> allDevices = [];

  // Step 2: Fetch each device (20-90%)
  for (int i = 0; i < totalDevices; i++) {
    final deviceId = summary.devices[i]['device_id'];
    final deviceName = summary.devices[i]['device_name'] ?? deviceId;

    final baseProgress = 20 + (70 * i / totalDevices);
    onProgress(baseProgress, 'Loading device ${i + 1}/$totalDevices: $deviceName');

    final deviceDetail = await sendCommand({
      'op': 'read',
      'type': 'device',
      'device_id': deviceId,
    });

    allDevices.add(deviceDetail.config);
  }

  // Step 3: Process (90-100%)
  onProgress(92.0, 'Processing data...');
  onProgress(100.0, 'Complete! Loaded $totalDevices devices');
}
```

### UI Components

#### Linear Progress Bar with Percentage

```dart
class ProgressIndicatorWithPercent extends StatelessWidget {
  final double progress;  // 0.0 to 100.0
  final String? statusText;

  const ProgressIndicatorWithPercent({
    required this.progress,
    this.statusText,
  });

  @override
  Widget build(BuildContext context) {
    return Column(
      mainAxisSize: MainAxisSize.min,
      children: [
        Row(
          children: [
            Expanded(
              child: LinearProgressIndicator(
                value: progress / 100,
                backgroundColor: Colors.grey[300],
                valueColor: AlwaysStoppedAnimation<Color>(
                  progress >= 100 ? Colors.green : Theme.of(context).primaryColor,
                ),
                minHeight: 8,
              ),
            ),
            SizedBox(width: 12),
            Text(
              '${progress.toStringAsFixed(0)}%',
              style: TextStyle(fontWeight: FontWeight.bold, fontSize: 16),
            ),
          ],
        ),
        if (statusText != null) ...[
          SizedBox(height: 8),
          Text(statusText!, style: TextStyle(color: Colors.grey[600], fontSize: 14)),
        ],
      ],
    );
  }
}
```

#### Circular Progress with Percentage (for dialogs)

```dart
class CircularProgressWithPercent extends StatelessWidget {
  final double progress;
  final String? statusText;
  final double size;

  const CircularProgressWithPercent({
    required this.progress,
    this.statusText,
    this.size = 100,
  });

  @override
  Widget build(BuildContext context) {
    return Column(
      mainAxisSize: MainAxisSize.min,
      children: [
        SizedBox(
          width: size,
          height: size,
          child: Stack(
            alignment: Alignment.center,
            children: [
              CircularProgressIndicator(
                value: progress / 100,
                strokeWidth: 8,
                backgroundColor: Colors.grey[300],
              ),
              Text(
                '${progress.toStringAsFixed(0)}%',
                style: TextStyle(fontWeight: FontWeight.bold, fontSize: size / 4),
              ),
            ],
          ),
        ),
        if (statusText != null) ...[
          SizedBox(height: 16),
          Text(statusText!, textAlign: TextAlign.center, style: TextStyle(fontSize: 14)),
        ],
      ],
    );
  }
}
```

#### Full Screen Loading Overlay

```dart
class LoadingOverlayWithProgress extends StatelessWidget {
  final double progress;
  final String title;
  final String? statusText;
  final VoidCallback? onCancel;

  const LoadingOverlayWithProgress({
    required this.progress,
    required this.title,
    this.statusText,
    this.onCancel,
  });

  @override
  Widget build(BuildContext context) {
    return Container(
      color: Colors.black54,
      child: Center(
        child: Card(
          margin: EdgeInsets.all(32),
          child: Padding(
            padding: EdgeInsets.all(24),
            child: Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                Text(title, style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold)),
                SizedBox(height: 24),
                CircularProgressWithPercent(progress: progress, statusText: statusText),
                if (onCancel != null) ...[
                  SizedBox(height: 24),
                  TextButton(onPressed: onCancel, child: Text('Cancel')),
                ],
              ],
            ),
          ),
        ),
      ),
    );
  }
}
```

### Progress for Each Screen/Operation

| Screen | Operation | Progress Stages |
|--------|-----------|-----------------|
| **Server Config** | Read config | Preparing (5%) → Sending (20%) → Receiving (70%) → Processing (95%) → Done (100%) |
| **Server Config** | Save config | Preparing (5%) → Sending (50%) → Confirming (80%) → Done (100%) |
| **Modbus Devices** | Load list | Preparing (5%) → Fetching (60%) → Processing (90%) → Done (100%) |
| **Device Detail** | Load device | Preparing (5%) → Fetching (70%) → Processing (95%) → Done (100%) |
| **Device Detail** | Save device | Preparing (5%) → Validating (15%) → Sending (60%) → Confirming (90%) → Done (100%) |
| **Backup** | Create backup | Preparing (5%) → Fetching configs (50%) → Packaging (80%) → Saving (95%) → Done (100%) |
| **Restore** | Restore backup | Preparing (5%) → Validating (15%) → Sending (70%) → Applying (90%) → Done (100%) |
| **OTA Update** | Check update | Checking (50%) → Done (100%) |
| **OTA Update** | Download | Preparing (5%) → Downloading (5-90%) → Validating (95%) → Done (100%) |
| **OTA Update** | Apply | Applying (50%) → Rebooting (100%) |
| **Factory Reset** | Reset | Confirming (10%) → Clearing (50%) → Resetting (80%) → Restarting (100%) |

### UI Display Examples

**Simple Command:**
```
┌─────────────────────────────────────┐
│       Loading Server Config         │
├─────────────────────────────────────┤
│                                     │
│  ████████████░░░░░░░░░░░░░░  45%   │
│                                     │
│  Receiving data...                  │
│                                     │
└─────────────────────────────────────┘
```

**Multi-Device Loading:**
```
┌─────────────────────────────────────┐
│       Loading Modbus Devices        │
├─────────────────────────────────────┤
│                                     │
│  ██████████████████░░░░░░░░  67%   │
│                                     │
│  Loading device 8/12: Power Meter   │
│                                     │
│  [Cancel]                           │
└─────────────────────────────────────┘
```

**OTA Download:**
```
┌─────────────────────────────────────┐
│        Firmware Update              │
├─────────────────────────────────────┤
│                                     │
│  ████████████████████░░░░░░  78%   │
│                                     │
│  Downloading... 1.52 MB / 1.94 MB   │
│                                     │
│  Estimated time: ~30 seconds        │
│                                     │
│  [Cancel Update]                    │
└─────────────────────────────────────┘
```

### Best Practices

**DO:**
- Always show percentage for operations > 1 second
- Update progress smoothly (not jumping)
- Show meaningful status text alongside percentage
- Allow cancel for long operations
- Show estimated time remaining for known-size operations

**DON'T:**
- Don't show indeterminate spinner for operations with known progress
- Don't update progress too frequently (max 10-20 updates/second)
- Don't show fake progress that doesn't reflect actual state
- Don't hide progress when user can't see if app is working

### Testing Checklist

- [ ] Server Config read shows progress 0-100%
- [ ] Server Config save shows progress 0-100%
- [ ] Device list loading shows progress with device count
- [ ] Large config (50+ registers) shows smooth progress
- [ ] OTA download shows bytes downloaded / total bytes
- [ ] Backup/restore shows progress for each step
- [ ] Cancel button works during long operations
- [ ] Progress bar doesn't jump backwards
- [ ] Status text updates meaningfully

---

## 5. P2: OTA Firmware Update UI

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

## 6. P2: Gateway Info UI

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

## 7. P3: Device Enable/Disable UI

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

## 8. P3: Production Mode Toggle (Developer Only)

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

## 9. P3: Timeout Optimization

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

## 10. P3: Streaming Delay Optimization

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
