# 🔧 STEP-BY-STEP INTEGRATION GUIDE

> **⚠️ ARCHIVED - October 2024**
>
> This document is archived and no longer applicable to current firmware.
>
> **Archived From:** v2.0.0 streaming fix integration guide
>
> **Reason:** Integration steps completed and changes merged into v2.0.0+
>
> **Current Documentation:** See [API Reference](../API_Reference/API.md) for
> current integration guidelines
>
> **Archive Info:** [ARCHIVE_INFO.md](ARCHIVE_INFO.md)

---

## 📋 Quick Summary

| Item           | Status        | File                                            |
| -------------- | ------------- | ----------------------------------------------- |
| Problem        | ❌ Found      | Format mismatch dalam streaming                 |
| Root Cause     | 🔍 Identified | ESP32 wraps response, Flutter expects unwrapped |
| Solution       | ✅ Provided   | Unwrap nested "data" field + detailed logging   |
| Implementation | 📝 Ready      | ble_controller_streaming_fixed.dart             |
| Guide          | 📚 Complete   | STREAMING_FIX_GUIDE.md                          |

---

## 🎯 Fase 1: Preparation (5 minutes)

### 1.1 Backup Original File

```bash
cd lib/core/controllers
cp ble_controller.dart ble_controller.dart.backup
cp ble_controller.dart ble_controller.dart.original
```

**Why**: Agar bisa rollback jika ada issue

### 1.2 Understand the Change

Read file: `STREAMING_FIX_GUIDE.md` - section "Masalah yang Ditemukan"

**Checklist**:

- [ ] Paham format mismatch issue
- [ ] Paham root cause di ESP32 vs Flutter
- [ ] Paham solusi (unwrap nested data)

---

## 🛠️ Fase 2: Add Logging Extension (10 minutes)

### 2.1 Locate Extension Point

Open `ble_controller.dart` dan cari class definition:

```dart
class BleController extends GetxController {
  // ... existing code
}
```

### 2.2 Add Extension SEBELUM Class Definition

Tambahkan di atas class `BleController`:

```dart
// ============================================================================
// LOGGING EXTENSIONS UNTUK STREAMING (TAMBAHAN UNTUK FIX)
// ============================================================================

extension StreamingLoggerExt on BleController {
  /// Log dengan prefix STREAMING untuk mudah filtered
  void _logStream(String message) {
    final timestamp = DateTime.now().toString().split('.')[0];
    AppHelpers.debugLog('[$timestamp] [STREAM] $message');
  }

  /// Log dengan prefix ERROR untuk streaming
  void _logStreamError(String message) {
    final timestamp = DateTime.now().toString().split('.')[0];
    AppHelpers.debugLog('[$timestamp] [STREAM_ERROR] $message');
  }

  /// Log data yang berhasil di-parse
  void _logStreamData(String address, String value) {
    final timestamp = DateTime.now().toString().split('.')[0];
    AppHelpers.debugLog('[$timestamp] [STREAM_DATA] $address = $value');
  }

  /// Log streaming status
  void _logStreamStatus(String status) {
    final timestamp = DateTime.now().toString().split('.')[0];
    AppHelpers.debugLog('[$timestamp] [STREAM_STATUS] $status');
  }
}

class BleController extends GetxController {
  // ... rest of code
}
```

**Verify**: File should compile tanpa error

---

## 🔄 Fase 3: Replace startDataStream Method (15 minutes)

### 3.1 Locate Original Method

Find method `startDataStream` (Line ~794):

```dart
Future<void> startDataStream(String type, String deviceId) async {
  // ... original code ...
}
```

### 3.2 Copy New Implementation

Copy dari file `ble_controller_streaming_fixed.dart` function
`startDataStreamFixed`

### 3.3 Replace Complete Method

```dart
/// FIXED VERSION - Handles wrapped response format dari ESP32
///
/// CHANGES:
/// 1. Unwrap nested "data" field dari response
/// 2. Add detailed logging di setiap step
/// 3. Handle format variations (Map vs List)
/// 4. Validate device_id matching
Future<void> startDataStream(String type, String deviceId) async {
  if (commandChar == null || responseChar == null) {
    errorMessage.value = 'Not connected';
    _logStreamError('BLE not connected, cannot start stream');
    return;
  }

  _logStreamStatus(
    'STARTING data stream for device: $deviceId, type: $type',
  );

  final startCommand = {"op": "read", "type": type, "device_id": deviceId};
  commandLoading.value = true;

  try {
    // ========================================================================
    // STEP 1: Cancel previous subscription jika ada
    // ========================================================================
    _logStream('Step 1: Cancelling previous subscription...');
    responseSubscription?.cancel();
    streamedData.clear();
    _logStream('Previous subscription cancelled, streamedData cleared');

    // ========================================================================
    // STEP 2: Create buffer dan setup listener untuk streaming data
    // ========================================================================
    _logStream('Step 2: Setting up response listener...');
    StringBuffer streamBuffer = StringBuffer();
    int chunkCount = 0;
    int totalDataPoints = 0;

    responseSubscription = responseChar!.lastValueStream.listen(
      (data) {
        chunkCount++;
        final chunk = utf8.decode(data, allowMalformed: true);

        _logStream(
          'Chunk #$chunkCount received (${data.length} bytes): "${chunk.length > 50 ? chunk.substring(0, 50) + "..." : chunk}"',
        );

        streamBuffer.write(chunk);

        if (chunk.contains('<END>')) {
          _logStream(
            'End marker detected! Processing complete buffer (size: ${streamBuffer.length})',
          );

          try {
            final rawBuffer = streamBuffer.toString();
            _logStream('Raw buffer received: "${rawBuffer.length} bytes"');

            final cleanedBuffer = rawBuffer
                .replaceAll('<END>', '')
                .replaceAll(RegExp(r'[\x00-\x1F\x7F]'), '')
                .trim();

            _logStream(
              'Buffer cleaned: "${cleanedBuffer.length} bytes" (removed <END> and control chars)',
            );

            if (cleanedBuffer.isEmpty) {
              throw Exception('Cleaned buffer is empty');
            }

            if (!cleanedBuffer.startsWith('{') && !cleanedBuffer.startsWith('[')) {
              throw Exception(
                'Malformed JSON structure - does not start with { or [',
              );
            }

            _logStream('Attempting to decode JSON...');
            final decoded = jsonDecode(cleanedBuffer);
            _logStream(
              'JSON decoded successfully. Type: ${decoded.runtimeType}',
            );

            _logStream(
              'Step 7: Parsing response format (handling nested structure)...',
            );

            Map<String, dynamic>? dataObject;

            if (decoded is Map<String, dynamic>) {
              _logStream(
                'Response is Map. Keys: ${decoded.keys.toList()}',
              );

              // ============================================================
              // KEY FIX: Check untuk "status" field (wrapped format)
              // ============================================================
              if (decoded.containsKey('status')) {
                _logStream('Detected "status" field: ${decoded['status']}');

                // Jika status == "data", extract nested "data" field
                if (decoded['status'] == 'data' && decoded.containsKey('data')) {
                  _logStream(
                    'Unwrapping nested "data" field from response...',
                  );
                  dataObject = Map<String, dynamic>.from(decoded['data'] as Map);
                  _logStream(
                    'Unwrapped data object keys: ${dataObject.keys.toList()}',
                  );
                }
                else if (decoded['status'] != 'data') {
                  _logStreamError(
                    'Received non-stream status: ${decoded['status']}. Full response: $decoded',
                  );
                  streamBuffer.clear();
                  return;
                }
                else {
                  _logStreamError(
                    'Status is "data" but no "data" field found. Full response: $decoded',
                  );
                  streamBuffer.clear();
                  return;
                }
              }
              else {
                _logStream(
                  'No "status" field - treating as direct data object',
                );
                dataObject = decoded;
              }
            }
            else if (decoded is List && decoded.isNotEmpty) {
              _logStream('Response is List with ${decoded.length} items');
              for (int i = 0; i < decoded.length; i++) {
                if (decoded[i] is Map<String, dynamic>) {
                  final itemData = Map<String, dynamic>.from(decoded[i] as Map);
                  _logStream(
                    'Processing list item #$i: ${itemData.keys.toList()}',
                  );
                  _processStreamDataObjectFixed(itemData, deviceId);
                }
              }
              streamBuffer.clear();
              return;
            }
            else {
              throw Exception(
                'Unexpected JSON root type: ${decoded.runtimeType}',
              );
            }

            if (dataObject != null) {
              _processStreamDataObjectFixed(dataObject, deviceId);
            }

            totalDataPoints++;
            _logStream(
              'Total data points processed so far: $totalDataPoints',
            );
          } catch (e) {
            _logStreamError(
              'JSON parsing error: $e. Raw buffer: ${streamBuffer.toString()}',
            );
          }

          streamBuffer.clear();
          _logStream(
            'Buffer cleared, ready for next frame (total chunks so far: $chunkCount)',
          );
        }
      },
      onError: (e) {
        _logStreamError('Notification stream error: $e');
        errorMessage.value = 'Stream error: $e';
      },
    );

    _logStream('Response listener setup complete');

    _logStream('Step 10: Enabling notifications on response characteristic...');
    await responseChar!.setNotifyValue(true);
    await Future.delayed(const Duration(milliseconds: 300));
    _logStream('Notifications enabled');

    _logStream('Step 11: Sending stream start command...');
    _logStream(
      'Command: ${jsonEncode(startCommand)}',
    );

    const chunkSize = 18;
    String jsonStr = jsonEncode(startCommand);
    int sentChunks = 0;

    final bool useWriteWithResponse =
        !(commandChar?.properties.writeWithoutResponse ?? false);

    for (int i = 0; i < jsonStr.length; i += chunkSize) {
      String chunk = jsonStr.substring(
        i,
        (i + chunkSize > jsonStr.length) ? jsonStr.length : i + chunkSize,
      );
      await commandChar!.write(
        utf8.encode(chunk),
        withoutResponse: !useWriteWithResponse,
      );
      sentChunks++;
      _logStream('Sent command chunk #$sentChunks: "$chunk"');
      await Future.delayed(const Duration(milliseconds: 50));
    }

    await commandChar!.write(
      utf8.encode('<END>'),
      withoutResponse: !useWriteWithResponse,
    );
    sentChunks++;
    _logStream('Sent <END> marker (chunk #$sentChunks)');

    _logStream(
      'Stream start command sent successfully (total $sentChunks chunks)',
    );

    SnackbarCustom.showSnackbar(
      '',
      'Streaming started for device: $deviceId',
      Colors.green,
      AppColor.whiteColor,
    );

    _logStreamStatus(
      'STREAMING ACTIVE for device: $deviceId. Listening for data...',
    );
  } catch (e) {
    _logStreamError('Error starting stream: $e');
    errorMessage.value = 'Error starting stream: $e';
    SnackbarCustom.showSnackbar(
      '',
      errorMessage.value,
      Colors.red,
      AppColor.whiteColor,
    );
  } finally {
    commandLoading.value = false;
  }
}

/// HELPER: Process individual data object dari stream
void _processStreamDataObjectFixed(
  Map<String, dynamic> dataObject,
  String expectedDeviceId,
) {
  try {
    if (dataObject.containsKey('device_id')) {
      final sourceDeviceId = dataObject['device_id']?.toString();
      if (sourceDeviceId != null && sourceDeviceId != expectedDeviceId) {
        _logStream(
          'Device ID mismatch: expected=$expectedDeviceId, received=$sourceDeviceId. Skipping.',
        );
        return;
      }
      _logStream('Device ID matched: $sourceDeviceId');
    }

    final address = dataObject['address']?.toString();
    final value = dataObject['value']?.toString();

    if (address == null || value == null) {
      _logStreamError(
        'Missing required fields! Data object keys: ${dataObject.keys.toList()}. Full object: $dataObject',
      );
      return;
    }

    streamedData[address] = value;

    final additionalInfo = <String>[];
    if (dataObject.containsKey('name')) {
      additionalInfo.add('name="${dataObject['name']}"');
    }
    if (dataObject.containsKey('type')) {
      additionalInfo.add('type="${dataObject['type']}"');
    }
    if (dataObject.containsKey('unit')) {
      additionalInfo.add('unit="${dataObject['unit']}"');
    }

    final info = additionalInfo.isNotEmpty ? ' (${additionalInfo.join(', ')})' : '';
    _logStreamData(address, '$value$info');

    _logStream(
      'Updated streamedData. Current entries: ${streamedData.length}',
    );
  } catch (e) {
    _logStreamError(
      'Error processing data object: $e. Object: $dataObject',
    );
  }
}
```

**Verify**: Method compile tanpa error

---

## 🔄 Fase 4: Update stopDataStream Method (5 minutes)

### 4.1 Locate Original stopDataStream

Find method `stopDataStream` (Line ~869):

```dart
Future<void> stopDataStream(String type) async {
  // ... original code ...
}
```

### 4.2 Replace dengan Fixed Version

```dart
/// FIXED VERSION dengan logging
Future<void> stopDataStream(String type) async {
  _logStreamStatus('STOPPING data stream...');

  final stopCommand = {"op": "read", "type": type, "device_id": "stop"};

  try {
    _logStream('Sending stop command: ${jsonEncode(stopCommand)}');
    await sendCommand(stopCommand);

    _logStream('Stop command sent, cancelling subscription...');
    responseSubscription?.cancel();

    _logStream(
      'Clearing ${streamedData.length} streamed data entries...',
    );
    streamedData.clear();

    _logStreamStatus('STREAMING STOPPED');

    SnackbarCustom.showSnackbar(
      '',
      'Streaming stopped',
      Colors.blue,
      AppColor.whiteColor,
    );
  } catch (e) {
    _logStreamError('Error stopping stream: $e');
  }
}
```

**Verify**: Method compile tanpa error

---

## ✨ Fase 5: Add Helper Methods (5 minutes)

### 5.1 Add di akhir Class Definition

```dart
/// Get streaming data snapshot untuk display di UI
Map<String, String> getStreamingDataSnapshot() {
  return Map<String, String>.from(streamedData);
}

/// Monitor streaming status - call ini periodically untuk logging
void monitorStreamingStatus() {
  _logStream('=== STREAMING STATUS REPORT ===');
  _logStream('Total data points received: ${streamedData.length}');
  _logStream('Last error: ${errorMessage.value}');

  if (streamedData.isNotEmpty) {
    _logStream(
      'Sample data: ${streamedData.entries.take(3).map((e) => '${e.key}=${e.value}').join(", ")}',
    );
  }
  _logStream('=== END STATUS REPORT ===');
}
```

---

## 🧪 Fase 6: Test Implementation (20 minutes)

### 6.1 Build & Run

```bash
# Terminal 1: Build app
flutter pub get
flutter run

# Terminal 2: Watch logs
adb logcat | grep STREAM
```

### 6.2 Manual Test: Streaming Start

```dart
// Di console atau test widget
void testStream() {
  final controller = Get.find<BleController>();

  // Start stream
  controller.startDataStream('data', 'Dca4cf');

  // Log monitoring
  Future.delayed(Duration(seconds: 3), () {
    controller.monitorStreamingStatus();
  });
}
```

**Expected Output**:

```
[STREAM_STATUS] STARTING data stream for device: Dca4cf, type: data
[STREAM] Step 2: Setting up response listener...
[STREAM] Step 10: Enabling notifications...
[STREAM] Step 11: Sending stream start command...
[STREAM_STATUS] STREAMING ACTIVE for device: Dca4cf...

[STREAM] Chunk #1 received (18 bytes)...
[STREAM] End marker detected!...
[STREAM_DATA] 0x3042 = 142...
[STREAM] Updated streamedData. Current entries: 1
```

### 6.3 Verify Data Updates

```dart
// Di periodic timer
Timer.periodic(Duration(seconds: 1), (_) {
  controller.monitorStreamingStatus();
});
```

**Expected**: Counter "Total data points received" increasing

### 6.4 Test Streaming Stop

```dart
// After 10 seconds of streaming
Future.delayed(Duration(seconds: 10), () {
  controller.stopDataStream('data');
});
```

**Expected**:

```
[STREAM_STATUS] STOPPING data stream...
[STREAM] Sending stop command...
[STREAM] Clearing X streamed data entries...
[STREAM_STATUS] STREAMING STOPPED
```

---

## ✅ Fase 7: Validation Checklist

### Pre-Integration

- [ ] Backup original file done
- [ ] Understand the issue
- [ ] Read STREAMING_FIX_GUIDE.md

### During Integration

- [ ] Extension added successfully
- [ ] startDataStream method replaced
- [ ] stopDataStream method updated
- [ ] Helper methods added
- [ ] Code compiles without errors

### Post-Integration

- [ ] App builds successfully
- [ ] Can connect to BLE device
- [ ] Streaming starts without error
- [ ] Logs show detailed steps
- [ ] Data points received and logged
- [ ] streamedData map updates correctly
- [ ] Streaming stops cleanly
- [ ] No memory leaks or crashes

### Log Verification

- [ ] [STREAM_STATUS] messages appear
- [ ] [STREAM] progress messages appear
- [ ] [STREAM_DATA] data updates appear
- [ ] No [STREAM_ERROR] messages (unless expected)
- [ ] Chunk received logs are present
- [ ] JSON parsing successful

---

## 📊 Comparison: Before vs After

### BEFORE (Original Code)

```
Streaming started... but no data in streamedData map
❌ Silent failure (no logs to understand why)
❌ Format mismatch not detected
❌ Hard to debug
```

### AFTER (Fixed Code)

```
[STREAM_STATUS] STARTING data stream...
[STREAM] Step 2: Setting up listener...
[STREAM] Chunk #1 received...
[STREAM] Unwrapping nested "data" field...
[STREAM_DATA] 0x3042 = 142
✅ Clear visibility into every step
✅ Format mismatch detected and handled
✅ Easy to debug any issues
```

---

## 🐛 If Something Goes Wrong

### Build Error

```bash
# Check imports
flutter pub get

# Clean build
flutter clean
flutter pub get
flutter run
```

### Runtime Error in Logs

Look for `[STREAM_ERROR]` lines:

```
[STREAM_ERROR] Missing required fields!
```

→ Baca detail di STREAMING_FIX_GUIDE.md section "Troubleshooting"

### Data Not Updating

```bash
# Monitor logs detail
adb logcat | grep "STREAM_DATA"

# If empty, check:
# 1. Device connected?
# 2. Streaming started? (look for STREAM_STATUS)
# 3. Data from ESP32? (Python test akan confirm)
```

---

## 🎯 Next: Using Fixed Streaming in UI

After integration, update your widgets:

```dart
// Widget yang display streaming data
class StreamingDataWidget extends StatelessWidget {
  @override
  Widget build(BuildContext context) {
    return Obx(() {
      final controller = Get.find<BleController>();

      return Column(
        children: [
          Text('Streaming Data Points: ${controller.streamedData.length}'),
          Expanded(
            child: ListView(
              children: controller.streamedData.entries.map((e) {
                return ListTile(
                  title: Text(e.key),
                  subtitle: Text('Value: ${e.value}'),
                );
              }).toList(),
            ),
          ),
        ],
      );
    });
  }
}
```

---

## 📞 Support / Questions

Jika ada issue:

1. Check logs untuk [STREAM_ERROR]
2. Refer ke STREAMING_FIX_GUIDE.md troubleshooting section
3. Verify ESP32 still sending data (gunakan Python test)
4. Check BLE connection stable
