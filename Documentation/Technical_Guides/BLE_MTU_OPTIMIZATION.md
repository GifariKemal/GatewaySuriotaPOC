# BLE MTU Optimization Guide for Mobile App

**Version:** 1.0 | **Date:** December 12, 2025 | **Target:** Flutter Mobile App
(`suriota_mobile_app`)

---

## Problem Statement

Current BLE transfer is **very slow** because:

- Chunk size hardcoded to **18 bytes**
- Delay between chunks is **100ms**
- No MTU negotiation (using default 23 bytes MTU)

**Example from device logs:**

```
[BLE] Fragment received (18 bytes, total: 36 bytes)
[BLE] Fragment received (18 bytes, total: 126 bytes)
[BLE] Fragment received (18 bytes, total: 216 bytes)
... (continues slowly)
```

**Impact:** Transferring 1KB data takes ~7-8 seconds instead of <0.5 seconds.

---

## Solution Overview

Implement **MTU negotiation** and **dynamic chunk sizing** to achieve **~25x
faster** transfer speed.

| Aspect       | Before           | After                      |
| ------------ | ---------------- | -------------------------- |
| Chunk Size   | Fixed 18 bytes   | Dynamic 20-200 bytes       |
| Chunk Delay  | Fixed 100ms      | Dynamic 15-50ms            |
| MTU          | Default 23 bytes | Negotiated up to 512 bytes |
| 1KB Transfer | ~7.7 seconds     | ~0.3 seconds               |

---

## Code Changes for `ble_controller.dart`

### 1. Update Constants (Replace lines 15-65)

```dart
class BleController extends GetxController {
  // ========== BLE COMMUNICATION CONSTANTS ==========

  /// Default BLE chunk size (conservative fallback)
  /// Used when MTU negotiation fails or returns low value
  static const int bleChunkSizeDefault = 20;

  /// Maximum BLE chunk size after successful MTU negotiation
  /// ESP32 firmware supports up to 244 bytes (BLE_MTU_MAX_SUPPORTED = 517, minus 3 ATT header)
  /// We use 200 to be safe across different Android/iOS devices
  static const int bleChunkSizeMax = 200;

  /// Minimum acceptable MTU for high-speed transfer
  static const int mtuMinimumForHighSpeed = 100;

  /// Target MTU to request during negotiation
  /// ESP32 firmware supports up to 517 bytes
  static const int mtuTargetSize = 512;

  /// Delay to ensure old subscription is fully cancelled before creating new one
  static const Duration subscriptionCleanupDelay = Duration(milliseconds: 50);

  /// Delay between BLE characteristic notifications to ensure stable transmission
  static const Duration notificationSetupDelay = Duration(milliseconds: 300);

  /// Delay between chunk transmissions for stable BLE communication
  /// Optimized: 15ms is sufficient for most modern devices (was 100ms)
  static const Duration chunkTransmissionDelay = Duration(milliseconds: 15);

  /// Delay between chunks for slower/older devices (fallback)
  static const Duration chunkTransmissionDelayFallback = Duration(milliseconds: 50);

  /// Delay before sending END delimiter
  static const Duration endDelimiterDelay = Duration(milliseconds: 50);

  /// Delay for batch processing of scanned devices (debounce)
  static const Duration batchProcessDelay = Duration(milliseconds: 300);

  /// Delay for UI updates (debounce)
  static const Duration uiUpdateDelay = Duration(milliseconds: 100);

  /// Buffer size limits for memory protection
  static const int maxBufferSize = 1024 * 100; // 100KB
  static const int maxPartialSize = 1024 * 10; // 10KB

  // ========== DYNAMIC BLE SETTINGS (set after MTU negotiation) ==========

  /// Current effective chunk size (updated after MTU negotiation)
  var _currentChunkSize = bleChunkSizeDefault;
  int get currentChunkSize => _currentChunkSize;

  /// Current chunk delay (updated based on negotiated MTU)
  Duration _currentChunkDelay = chunkTransmissionDelayFallback;
  Duration get currentChunkDelay => _currentChunkDelay;

  /// Negotiated MTU size (observable for UI)
  var negotiatedMtu = 23.obs;  // Default BLE MTU
  var mtuNegotiationSuccess = false.obs;

  // ... rest of existing state variables ...
```

**DELETE this old constant:**

```dart
// DELETE THIS LINE:
static const int bleChunkSize = 18;
```

---

### 2. Add MTU Negotiation Methods (Add after `connectToDevice` method, around line 661)

```dart
  // ========== MTU NEGOTIATION METHODS ==========

  /// Negotiates MTU with connected device for optimal transfer speed
  ///
  /// Returns the effective payload size (MTU - 3 bytes ATT header)
  /// Falls back to conservative settings if negotiation fails
  Future<int> _negotiateMtu(BluetoothDevice device) async {
    try {
      AppHelpers.debugLog('Starting MTU negotiation (requesting $mtuTargetSize bytes)...');

      // Request high MTU - flutter_blue_plus handles platform differences
      final mtu = await device.requestMtu(mtuTargetSize);

      // Effective payload = MTU - 3 (ATT protocol overhead)
      final effectivePayload = mtu - 3;

      negotiatedMtu.value = mtu;

      AppHelpers.debugLog('MTU negotiation success: $mtu bytes (effective payload: $effectivePayload bytes)');

      // Determine optimal chunk size based on negotiated MTU
      if (effectivePayload >= mtuMinimumForHighSpeed) {
        // High-speed mode: use larger chunks with shorter delay
        _currentChunkSize = effectivePayload.clamp(bleChunkSizeDefault, bleChunkSizeMax);
        _currentChunkDelay = chunkTransmissionDelay;
        mtuNegotiationSuccess.value = true;

        AppHelpers.debugLog('High-speed mode enabled: chunk=$_currentChunkSize bytes, delay=${_currentChunkDelay.inMilliseconds}ms');
      } else {
        // Fallback mode: conservative settings for older devices
        _currentChunkSize = effectivePayload.clamp(18, bleChunkSizeDefault);
        _currentChunkDelay = chunkTransmissionDelayFallback;
        mtuNegotiationSuccess.value = false;

        AppHelpers.debugLog('Fallback mode (low MTU): chunk=$_currentChunkSize bytes, delay=${_currentChunkDelay.inMilliseconds}ms');
      }

      return effectivePayload;

    } catch (e) {
      AppHelpers.debugLog('MTU negotiation failed: $e - using conservative defaults');

      // Use most conservative settings on failure
      _currentChunkSize = 18;
      _currentChunkDelay = const Duration(milliseconds: 100);
      negotiatedMtu.value = 23;
      mtuNegotiationSuccess.value = false;

      return 20;
    }
  }

  /// Resets MTU settings to defaults (call on disconnect)
  void _resetMtuSettings() {
    _currentChunkSize = bleChunkSizeDefault;
    _currentChunkDelay = chunkTransmissionDelayFallback;
    negotiatedMtu.value = 23;
    mtuNegotiationSuccess.value = false;
    AppHelpers.debugLog('MTU settings reset to defaults');
  }

  /// Get connection quality description based on MTU (for UI display)
  String get connectionQuality {
    if (!mtuNegotiationSuccess.value) return 'Standard';
    if (negotiatedMtu.value >= 500) return 'Excellent';
    if (negotiatedMtu.value >= 200) return 'Good';
    return 'Fair';
  }

  /// Get estimated transfer speed multiplier vs default (for UI display)
  String get speedMultiplier {
    final multiplier = _currentChunkSize / 18.0;
    return '${multiplier.toStringAsFixed(1)}x';
  }
```

---

### 3. Update `connectToDevice` Method

**Find this section (around line 586-606):**

```dart
      await deviceModel.device.connect();
      connectedDevice.value = deviceModel.device;
      deviceModel.isConnected.value = true;

      // Add to connection history
      addOrUpdateHistory(deviceModel);

      final deviceId = deviceModel.device.remoteId.toString();
      _deviceCache[deviceId] = deviceModel;
      AppHelpers.debugLog('Device re-added to cache: $deviceId');

      AppHelpers.debugLog(
        'Device connected successfully: ${deviceModel.device.remoteId}',
      );

      // Discover services
      List<BluetoothService> services = await deviceModel.device.discoverServices();
```

**Replace with:**

```dart
      await deviceModel.device.connect();
      connectedDevice.value = deviceModel.device;
      deviceModel.isConnected.value = true;

      // Add to connection history
      addOrUpdateHistory(deviceModel);

      final deviceId = deviceModel.device.remoteId.toString();
      _deviceCache[deviceId] = deviceModel;
      AppHelpers.debugLog('Device re-added to cache: $deviceId');

      AppHelpers.debugLog(
        'Device connected successfully: ${deviceModel.device.remoteId}',
      );

      // ========== NEW: MTU NEGOTIATION ==========
      message.value = 'Negotiating MTU...';
      await _negotiateMtu(deviceModel.device);
      // ==========================================

      // Discover services
      message.value = 'Discovering services...';
      List<BluetoothService> services = await deviceModel.device.discoverServices();
```

---

### 4. Update `disconnectFromDevice` Method

**Find this section (around line 706):**

```dart
      await deviceModel.device.disconnect();

      // Don't nullify characteristics if streaming is active
```

**Replace with:**

```dart
      await deviceModel.device.disconnect();

      // ========== NEW: RESET MTU SETTINGS ==========
      _resetMtuSettings();
      // =============================================

      // Don't nullify characteristics if streaming is active
```

---

### 5. Update `sendCommand` Method - Use Dynamic Chunk Size

**Find this section (around line 1408-1426):**

```dart
      // Send command in chunks
      StringBuffer sentCommand = StringBuffer();
      for (int i = 0; i < jsonStr.length; i += bleChunkSize) {
        String chunk = jsonStr.substring(
          i,
          (i + bleChunkSize > jsonStr.length)
              ? jsonStr.length
              : i + bleChunkSize,
        );
        sentCommand.write(chunk);
        await cmd.write(
          utf8.encode(chunk),
          withoutResponse: !useWriteWithResponse,
        );
        AppHelpers.debugLog('Sent chunk: $chunk');
        currentChunk++;
        commandProgress.value = currentChunk / totalChunks;
        // Delay for stable BLE transmission
        await Future.delayed(chunkTransmissionDelay);
      }
```

**Replace with:**

```dart
      // Send command in chunks - USE DYNAMIC CHUNK SIZE
      StringBuffer sentCommand = StringBuffer();
      for (int i = 0; i < jsonStr.length; i += _currentChunkSize) {
        String chunk = jsonStr.substring(
          i,
          (i + _currentChunkSize > jsonStr.length)
              ? jsonStr.length
              : i + _currentChunkSize,
        );
        sentCommand.write(chunk);
        await cmd.write(
          utf8.encode(chunk),
          withoutResponse: !useWriteWithResponse,
        );
        AppHelpers.debugLog('Sent chunk (${chunk.length} bytes)');
        currentChunk++;
        commandProgress.value = currentChunk / totalChunks;
        // USE DYNAMIC DELAY based on MTU negotiation
        await Future.delayed(_currentChunkDelay);
      }
```

**Also update `totalChunks` calculation (around line 1400):**

```dart
// BEFORE:
int totalChunks = (jsonStr.length / bleChunkSize).ceil() + 1;

// AFTER:
int totalChunks = (jsonStr.length / _currentChunkSize).ceil() + 1;
```

---

### 6. Update `sendReadCommand` Method

**Find and replace all occurrences (around line 1662 and 1913-1927):**

```dart
// BEFORE:
const chunkSize = bleChunkSize;

// AFTER:
final chunkSize = _currentChunkSize;
```

```dart
// BEFORE:
await Future.delayed(chunkTransmissionDelay);

// AFTER:
await Future.delayed(_currentChunkDelay);
```

---

### 7. Update Streaming Methods

**In `startDataStream` (around line 2239-2254):**

```dart
// BEFORE:
const chunkSize = 18;
await Future.delayed(const Duration(milliseconds: 50));

// AFTER:
final chunkSize = _currentChunkSize;
await Future.delayed(_currentChunkDelay);
```

**In `startEnhancedStreaming` (around line 2578-2593):**

```dart
// BEFORE:
const chunkSize = 18;
await Future.delayed(const Duration(milliseconds: 50));

// AFTER:
final chunkSize = _currentChunkSize;
await Future.delayed(_currentChunkDelay);
```

**In `_sendCommandManually` (around line 2302-2313):**

```dart
// BEFORE:
for (int i = 0; i < jsonStr.length; i += bleChunkSize) {
  String chunk = jsonStr.substring(
    i,
    (i + bleChunkSize > jsonStr.length) ? jsonStr.length : i + bleChunkSize,
  );
  // ...
  await Future.delayed(const Duration(milliseconds: 50));
}

// AFTER:
for (int i = 0; i < jsonStr.length; i += _currentChunkSize) {
  String chunk = jsonStr.substring(
    i,
    (i + _currentChunkSize > jsonStr.length) ? jsonStr.length : i + _currentChunkSize,
  );
  // ...
  await Future.delayed(_currentChunkDelay);
}
```

---

## Summary: Files to Modify

| File                                       | Changes           |
| ------------------------------------------ | ----------------- |
| `lib/core/controllers/ble_controller.dart` | All changes above |

---

## Quick Find & Replace

Use IDE find & replace for quick updates:

| Find                                           | Replace With                          |
| ---------------------------------------------- | ------------------------------------- |
| `bleChunkSize`                                 | `_currentChunkSize`                   |
| `const chunkSize = 18`                         | `final chunkSize = _currentChunkSize` |
| `chunkTransmissionDelay` (in await)            | `_currentChunkDelay`                  |
| `Duration(milliseconds: 50)` (in chunk loops)  | `_currentChunkDelay`                  |
| `Duration(milliseconds: 100)` (in chunk loops) | `_currentChunkDelay`                  |

---

## Testing Checklist

After implementing changes, test on:

- [ ] **Modern Android** (Android 10+) - Should get high MTU (~500 bytes)
- [ ] **Older Android** (Android 7-9) - May get lower MTU, should fallback
      gracefully
- [ ] **iOS** - Usually auto-negotiates high MTU
- [ ] **Large data transfer** (backup config ~50KB) - Should be significantly
      faster
- [ ] **Small commands** (read production_mode) - Should work as before
- [ ] **Streaming data** - Should be stable with new chunk sizes
- [ ] **Reconnection** - MTU should re-negotiate properly

---

## Expected Results

**Before optimization (18 bytes, 100ms delay):**

```
[BLE] Fragment received (18 bytes, total: 36 bytes)
[BLE] Fragment received (18 bytes, total: 126 bytes)
... (77 chunks for 1.4KB, ~7.7 seconds)
```

**After optimization (200 bytes, 15ms delay):**

```
[BLE] Fragment received (200 bytes, total: 200 bytes)
[BLE] Fragment received (200 bytes, total: 400 bytes)
... (7 chunks for 1.4KB, ~0.1 seconds)
```

---

## Firmware Compatibility

ESP32 firmware already supports high MTU:

- `BLE_MTU_MAX_SUPPORTED = 517` bytes
- `CHUNK_SIZE = 244` bytes for sending
- `FRAGMENT_DELAY_MS = 10` ms

No firmware changes needed.

---

## Troubleshooting

**Q: MTU negotiation fails on some devices** A: The code automatically falls
back to conservative settings (18 bytes, 100ms). Check
`mtuNegotiationSuccess.value` in debug logs.

**Q: Data corruption after increasing chunk size** A: Reduce `bleChunkSizeMax`
from 200 to 150 or 100. Some older Bluetooth chips have issues with large
packets.

**Q: iOS not getting high MTU** A: iOS handles MTU differently. Try calling
`requestMtu` after a short delay (500ms) post-connection.

---

## Contact

For questions about firmware BLE implementation:

- Check `Main/BLEManager.cpp` for ESP32 BLE handling
- Check `Main/BLEManager.h` for MTU constants

---

**Document prepared by Claude Code** | December 12, 2025
