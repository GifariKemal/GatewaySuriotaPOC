# Flutter BLE Cancel Implementation Guide

**Version:** 1.0.9 | **Date:** January 2026 | **Status:** Production

---

## Overview

This guide explains how to implement the BLE transmission cancellation feature in
Flutter/Dart mobile applications. The firmware (v1.0.9+) supports a `<CANCEL>`
command that stops ongoing chunked BLE transmissions.

---

## Protocol Summary

### Cancel Command

**Mobile sends:**

```
<CANCEL>
```

**Gateway responds:**

```
<ACK>
```

### Cancelled Transmission Marker

If a transmission is cancelled mid-stream, the gateway sends:

```
<CANCELLED>
```

Instead of the normal `<END>` marker.

---

## Implementation

### 1. BLE Service Class

```dart
import 'dart:async';
import 'dart:convert';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';

class BleGatewayService {
  // BLE UUIDs (must match firmware)
  static const String serviceUuid = '00001830-0000-1000-8000-00805f9b34fb';
  static const String commandCharUuid = '11111111-1111-1111-1111-111111111101';
  static const String responseCharUuid = '11111111-1111-1111-1111-111111111102';

  BluetoothDevice? _device;
  BluetoothCharacteristic? _commandChar;
  BluetoothCharacteristic? _responseChar;
  StreamSubscription? _notificationSubscription;

  // Response buffer management
  final StringBuffer _responseBuffer = StringBuffer();
  Completer<Map<String, dynamic>?>? _responseCompleter;
  bool _transmissionActive = false;
  bool _cancelled = false;

  // ========================================
  // CONNECTION MANAGEMENT
  // ========================================

  bool get isTransmissionActive => _transmissionActive;

  Future<void> connect(BluetoothDevice device) async {
    _device = device;
    await _device!.connect();

    final services = await _device!.discoverServices();
    final service = services.firstWhere(
      (s) => s.uuid.toString() == serviceUuid,
    );

    _commandChar = service.characteristics.firstWhere(
      (c) => c.uuid.toString() == commandCharUuid,
    );
    _responseChar = service.characteristics.firstWhere(
      (c) => c.uuid.toString() == responseCharUuid,
    );

    // Enable notifications
    await _responseChar!.setNotifyValue(true);
    _notificationSubscription = _responseChar!.value.listen(_onNotification);
  }

  Future<void> disconnect() async {
    await cancelCommand();
    await _notificationSubscription?.cancel();
    await _device?.disconnect();
  }

  // ========================================
  // CANCEL COMMAND (v1.0.9+)
  // ========================================

  /// Cancel any ongoing BLE transmission
  /// Call this when:
  /// - User navigates to another page
  /// - User sends a new command before previous completes
  /// - App goes to background
  /// - Timeout occurs
  Future<void> cancelCommand() async {
    if (!_transmissionActive) {
      return; // Nothing to cancel
    }

    _cancelled = true;
    _transmissionActive = false;
    _responseBuffer.clear();

    // Complete any waiting completer with null
    if (_responseCompleter != null && !_responseCompleter!.isCompleted) {
      _responseCompleter!.complete(null);
    }
    _responseCompleter = null;

    // Send <CANCEL> to firmware
    try {
      await _sendRaw('<CANCEL>');
      print('[BLE] Cancel command sent');
    } catch (e) {
      print('[BLE] Error sending cancel: $e');
    }
  }

  /// Send raw data without chunking (for control commands like <CANCEL>)
  Future<void> _sendRaw(String data) async {
    if (_commandChar == null) {
      throw Exception('Not connected');
    }
    await _commandChar!.write(utf8.encode(data), withoutResponse: false);
  }

  // ========================================
  // COMMAND SENDING
  // ========================================

  /// Send a command and wait for response
  /// Automatically cancels any previous pending command
  Future<Map<String, dynamic>?> sendCommand(
    Map<String, dynamic> command, {
    Duration timeout = const Duration(seconds: 30),
  }) async {
    // Cancel any previous command first
    await cancelCommand();

    // Reset state
    _cancelled = false;
    _transmissionActive = true;
    _responseBuffer.clear();
    _responseCompleter = Completer<Map<String, dynamic>?>();

    try {
      // Send command as JSON
      final jsonStr = jsonEncode(command);
      await _sendChunked(jsonStr);

      // Wait for response with timeout
      final response = await _responseCompleter!.future.timeout(
        timeout,
        onTimeout: () {
          print('[BLE] Command timeout after ${timeout.inSeconds}s');
          cancelCommand();
          return null;
        },
      );

      return response;
    } catch (e) {
      print('[BLE] Error sending command: $e');
      _transmissionActive = false;
      return null;
    }
  }

  /// Send data in chunks (for large commands)
  Future<void> _sendChunked(String data) async {
    const chunkSize = 244; // MTU-safe chunk size
    final bytes = utf8.encode(data);

    // Send <START> marker
    await _sendRaw('<START>');
    await Future.delayed(const Duration(milliseconds: 10));

    // Send data chunks
    for (var i = 0; i < bytes.length; i += chunkSize) {
      if (_cancelled) {
        return; // Stop if cancelled
      }

      final end = (i + chunkSize > bytes.length) ? bytes.length : i + chunkSize;
      final chunk = bytes.sublist(i, end);
      await _commandChar!.write(chunk, withoutResponse: false);
      await Future.delayed(const Duration(milliseconds: 10));
    }

    // Send <END> marker
    await _sendRaw('<END>');
  }

  // ========================================
  // NOTIFICATION HANDLING
  // ========================================

  void _onNotification(List<int> data) {
    if (data.isEmpty) return;

    final chunk = utf8.decode(data, allowMalformed: true);

    // Handle control markers
    if (chunk == '<ACK>') {
      print('[BLE] Cancel acknowledged by gateway');
      return;
    }

    if (chunk == '<CANCELLED>') {
      print('[BLE] Transmission was cancelled');
      _transmissionActive = false;
      _responseBuffer.clear();
      return;
    }

    // Ignore notifications if cancelled or no active transmission
    if (_cancelled || !_transmissionActive) {
      print('[BLE] Ignoring chunk - cancelled or inactive');
      return;
    }

    // Handle progress notifications (filter them out)
    if (chunk.contains('"type":"download_progress"') ||
        chunk.contains('"type":"upload_progress"') ||
        chunk.contains('"type":"ota_progress"')) {
      // Process progress notification separately
      _handleProgressNotification(chunk);
      return;
    }

    // Check for end marker
    if (chunk == '<END>') {
      _completeResponse();
      return;
    }

    // Accumulate response chunks
    _responseBuffer.write(chunk);
  }

  void _handleProgressNotification(String chunk) {
    try {
      final json = jsonDecode(chunk);
      final type = json['type'];
      final percent = json['percent'] ?? 0;

      print('[BLE] Progress: $type - $percent%');

      // Optionally emit to a stream for UI updates
      // _progressController.add(ProgressEvent(type, percent));
    } catch (e) {
      // Ignore parse errors for progress notifications
    }
  }

  void _completeResponse() {
    _transmissionActive = false;

    final responseStr = _responseBuffer.toString();
    _responseBuffer.clear();

    if (responseStr.isEmpty) {
      _responseCompleter?.complete(null);
      return;
    }

    try {
      final json = jsonDecode(responseStr) as Map<String, dynamic>;
      _responseCompleter?.complete(json);
    } catch (e) {
      print('[BLE] JSON parse error: $e');
      _responseCompleter?.complete(null);
    }
  }
}
```

### 2. Page Integration

```dart
import 'package:flutter/material.dart';

class DeviceListPage extends StatefulWidget {
  @override
  _DeviceListPageState createState() => _DeviceListPageState();
}

class _DeviceListPageState extends State<DeviceListPage> {
  final BleGatewayService _bleService = BleGatewayService();
  List<Map<String, dynamic>> _devices = [];
  bool _loading = false;

  @override
  void initState() {
    super.initState();
    _loadDevices();
  }

  @override
  void dispose() {
    // CRITICAL: Cancel any pending command when leaving page
    _bleService.cancelCommand();
    super.dispose();
  }

  Future<void> _loadDevices() async {
    setState(() => _loading = true);

    final response = await _bleService.sendCommand({
      'op': 'read',
      'type': 'device',
    });

    if (response != null && response['status'] == 'ok') {
      setState(() {
        _devices = List<Map<String, dynamic>>.from(response['devices'] ?? []);
        _loading = false;
      });
    } else {
      setState(() => _loading = false);
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Failed to load devices')),
      );
    }
  }

  @override
  Widget build(BuildContext context) {
    return WillPopScope(
      // Handle back button - cancel if transmission active
      onWillPop: () async {
        if (_bleService.isTransmissionActive) {
          final shouldLeave = await showDialog<bool>(
            context: context,
            builder: (context) => AlertDialog(
              title: const Text('Cancel Operation?'),
              content: const Text(
                'A command is still in progress. Cancel and leave?',
              ),
              actions: [
                TextButton(
                  onPressed: () => Navigator.pop(context, false),
                  child: const Text('STAY'),
                ),
                TextButton(
                  onPressed: () {
                    _bleService.cancelCommand();
                    Navigator.pop(context, true);
                  },
                  child: const Text('LEAVE'),
                ),
              ],
            ),
          );
          return shouldLeave ?? false;
        }
        return true;
      },
      child: Scaffold(
        appBar: AppBar(
          title: const Text('Devices'),
          actions: [
            if (_loading)
              IconButton(
                icon: const Icon(Icons.cancel),
                onPressed: () {
                  _bleService.cancelCommand();
                  setState(() => _loading = false);
                },
                tooltip: 'Cancel',
              ),
          ],
        ),
        body: _loading
            ? const Center(child: CircularProgressIndicator())
            : ListView.builder(
                itemCount: _devices.length,
                itemBuilder: (context, index) {
                  final device = _devices[index];
                  return ListTile(
                    title: Text(device['device_name'] ?? 'Unknown'),
                    subtitle: Text(device['device_id'] ?? ''),
                    onTap: () {
                      // Cancel current and navigate
                      _bleService.cancelCommand();
                      Navigator.push(
                        context,
                        MaterialPageRoute(
                          builder: (_) => DeviceDetailPage(device: device),
                        ),
                      );
                    },
                  );
                },
              ),
      ),
    );
  }
}
```

### 3. Global Navigation Observer

```dart
/// Add this to your MaterialApp to auto-cancel on any navigation
class BleNavigationObserver extends NavigatorObserver {
  final BleGatewayService bleService;

  BleNavigationObserver(this.bleService);

  @override
  void didPush(Route<dynamic> route, Route<dynamic>? previousRoute) {
    // Cancel when pushing new route
    bleService.cancelCommand();
    super.didPush(route, previousRoute);
  }

  @override
  void didPop(Route<dynamic> route, Route<dynamic>? previousRoute) {
    // Cancel when popping route
    bleService.cancelCommand();
    super.didPop(route, previousRoute);
  }

  @override
  void didReplace({Route<dynamic>? newRoute, Route<dynamic>? oldRoute}) {
    // Cancel when replacing route
    bleService.cancelCommand();
    super.didReplace(newRoute: newRoute, oldRoute: oldRoute);
  }
}

// Usage in MaterialApp:
MaterialApp(
  navigatorObservers: [
    BleNavigationObserver(bleService),
  ],
  // ...
)
```

### 4. App Lifecycle Handler

```dart
/// Handle app going to background
class BleLifecycleObserver extends WidgetsBindingObserver {
  final BleGatewayService bleService;

  BleLifecycleObserver(this.bleService);

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    if (state == AppLifecycleState.paused ||
        state == AppLifecycleState.inactive) {
      // Cancel transmission when app goes to background
      bleService.cancelCommand();
    }
  }
}

// Register in main.dart or app state:
WidgetsBinding.instance.addObserver(BleLifecycleObserver(bleService));
```

---

## Response Markers Reference

| Marker | Direction | Description |
|--------|-----------|-------------|
| `<START>` | Mobile -> Gateway | Start of chunked command |
| `<END>` | Both | End of chunked transmission |
| `<CANCEL>` | Mobile -> Gateway | Cancel ongoing transmission |
| `<ACK>` | Gateway -> Mobile | Cancel acknowledged |
| `<CANCELLED>` | Gateway -> Mobile | Transmission was cancelled |

---

## Error Handling

### Cancel Error Codes

| Code | Domain | Description |
|------|--------|-------------|
| 350 | BLE | Command cancelled by client |
| 351 | BLE | Previous command still running |
| 352 | BLE | Cancel failed |

### Best Practices

1. **Always cancel on page dispose** - Prevents orphaned transmissions
2. **Use WillPopScope** - Give users choice to stay or leave
3. **Show cancel button during loading** - Let users abort long operations
4. **Implement timeouts** - Auto-cancel after 30s to prevent hanging
5. **Filter progress notifications** - Don't mix with response JSON

---

## Testing Checklist

- [ ] Navigate away during device list loading (should cancel)
- [ ] Send new command while previous is responding (should cancel first)
- [ ] Tap back button during backup download (should prompt)
- [ ] App goes to background during transmission (should cancel)
- [ ] Rapid cancel/send cycles (should not crash)
- [ ] Cancel at various transmission progress points
- [ ] Verify `<ACK>` received after `<CANCEL>`
- [ ] Verify `<CANCELLED>` marker handled correctly

---

## Compatibility

| Firmware Version | Cancel Support |
|------------------|----------------|
| < 1.0.9 | No |
| >= 1.0.9 | Yes |

For firmware < 1.0.9, implement Level 1 (mobile-side buffer management only):

```dart
// Fallback for old firmware - just ignore incoming chunks
void cancelCommandFallback() {
  _cancelled = true;
  _transmissionActive = false;
  _responseBuffer.clear();
  // Don't send <CANCEL> - old firmware doesn't understand it
}
```

---

**Document Version:** 1.0 | **Last Updated:** January 2026

**SURIOTA R&D Team** | support@suriota.com
