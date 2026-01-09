# BLE Cancel Command Feature

**Version:** 1.0.9 | **Date:** January 2026 | **Status:** Implemented

---

## Problem Statement

When mobile app sends a BLE command that triggers a large response (e.g., backup,
device list with many registers), the response is sent in chunks/fragments due to
MTU limitations. If the user:

1. Navigates to another page
2. Sends a new command before previous completes
3. Closes the app or disconnects

The gateway continues sending chunks of the old response, which can:
- Mix with new command responses
- Waste BLE bandwidth
- Cause parsing errors on mobile
- Lead to poor user experience

---

## Proposed Solution: 3-Level Approach

### Level 1: Mobile-Side Buffer Management (Immediate)

**No firmware changes required.** Mobile app handles cancellation internally.

```dart
class BleResponseManager {
  String? _currentCommandId;
  List<String> _responseBuffer = [];
  bool _isCancelled = false;

  /// Generate unique command ID
  String _generateCommandId() {
    return DateTime.now().millisecondsSinceEpoch.toString();
  }

  /// Start a new command, cancel any pending
  Future<Map<String, dynamic>?> sendCommand(
    Map<String, dynamic> command,
  ) async {
    // Cancel previous command
    _isCancelled = true;
    await Future.delayed(Duration(milliseconds: 100));

    // Start new command
    _currentCommandId = _generateCommandId();
    _responseBuffer.clear();
    _isCancelled = false;

    // Send command...
    final myCommandId = _currentCommandId;

    // When receiving chunks, check if still valid
    // See notification handler below
  }

  /// Handle incoming BLE notification
  void onNotification(String chunk) {
    // Ignore if cancelled or different command
    if (_isCancelled) {
      print('Ignoring chunk - command cancelled');
      return;
    }

    if (chunk == '<END>') {
      // Process complete response
      _processResponse();
    } else {
      _responseBuffer.add(chunk);
    }
  }

  /// Clear buffer on page change
  void cancelPendingCommand() {
    _isCancelled = true;
    _responseBuffer.clear();
    _currentCommandId = null;
  }
}
```

**Pros:**
- No firmware update needed
- Can implement immediately
- Works with existing gateways

**Cons:**
- Gateway still sends all chunks (bandwidth waste)
- Chunks still processed by BLE stack

---

### Level 2: Firmware Cancel Command (Recommended)

Add a special `<CANCEL>` command that tells firmware to stop sending.

#### BLE Protocol Update

**Cancel Command (Mobile → Gateway):**
```
<CANCEL>
```

**Cancel Acknowledgment (Gateway → Mobile):**
```json
{"status":"cancelled"}
```
or simply:
```
<ACK>
```

#### Firmware Implementation

```cpp
// In BLEManager.cpp - onWrite handler

void BLEManager::onCharacteristicWrite(const String& data) {
  if (data == "<CANCEL>") {
    // Stop any ongoing transmission
    cancelOngoingTransmission();

    // Clear output buffer
    clearResponseBuffer();

    // Send acknowledgment
    sendNotification("<ACK>");

    LOG_BLE_INFO("[BLE] Command cancelled by client\n");
    return;
  }

  // Normal command processing...
}

void BLEManager::cancelOngoingTransmission() {
  // Set flag to stop chunked sending
  transmissionCancelled = true;

  // Clear any pending chunks in queue
  while (!responseQueue.empty()) {
    responseQueue.pop();
  }
}
```

**In sendResponse() - check cancellation:**
```cpp
void BLEManager::sendResponse(const JsonDocument& response) {
  String jsonStr;
  serializeJson(response, jsonStr);

  // Send in chunks
  for (int i = 0; i < jsonStr.length(); i += mtuSize) {
    // Check if cancelled
    if (transmissionCancelled) {
      LOG_BLE_INFO("[BLE] Transmission cancelled, stopping\n");
      transmissionCancelled = false;
      return;
    }

    String chunk = jsonStr.substring(i, min(i + mtuSize, jsonStr.length()));
    sendNotification(chunk);
    vTaskDelay(pdMS_TO_TICKS(20)); // Allow BLE stack to process
  }

  sendNotification("<END>");
}
```

#### Mobile Implementation

```dart
class BleGatewayService {
  bool _transmissionActive = false;

  /// Cancel any ongoing command
  Future<void> cancelCommand() async {
    if (_transmissionActive) {
      await _sendRaw('<CANCEL>');
      _responseBuffer.clear();
      _transmissionActive = false;
    }
  }

  /// Send command with automatic cancellation
  Future<Map<String, dynamic>?> sendCommand(
    Map<String, dynamic> command,
  ) async {
    // Cancel previous if any
    await cancelCommand();

    _transmissionActive = true;

    try {
      // Send command chunks...
      final response = await _waitForResponse();
      return response;
    } finally {
      _transmissionActive = false;
    }
  }
}
```

**Pros:**
- Stops unnecessary BLE traffic
- Cleaner protocol
- Saves battery on both sides

**Cons:**
- Requires firmware update
- Need to handle edge cases

---

### Level 3: Command Sequencing with Transaction ID (Advanced)

Add transaction IDs to track command-response pairs.

#### Enhanced Protocol

**Request with Transaction ID:**
```json
{
  "txn": "ABC123",
  "op": "read",
  "type": "device"
}
```

**Response includes Transaction ID:**
```json
{
  "txn": "ABC123",
  "status": "ok",
  "devices": [...]
}
```

**Cancel specific transaction:**
```json
{
  "op": "cancel",
  "txn": "ABC123"
}
```

#### Benefits

1. **Precise Matching** - Mobile knows exactly which response belongs to which request
2. **Concurrent Commands** - Can have multiple commands in flight (future)
3. **Timeout Handling** - Easy to detect stuck commands
4. **Debugging** - Clear audit trail

#### Implementation Complexity

| Component | Effort |
|-----------|--------|
| Mobile App | Medium |
| Firmware BLEManager | Medium |
| Firmware CRUDHandler | Low |
| Testing | High |

---

## Recommended Implementation Plan

### Phase 1: Mobile-Side Only (Immediate)

Implement Level 1 in mobile app:

```dart
// In BLE service
void onPageChange() {
  bleService.cancelPendingCommand();
}

// In each page's dispose
@override
void dispose() {
  bleService.cancelPendingCommand();
  super.dispose();
}
```

### Phase 2: Firmware Cancel Command (v1.0.9)

Add `<CANCEL>` support to firmware:

1. Update BLEManager to handle `<CANCEL>`
2. Add transmission flag check in sendResponse
3. Update mobile app to send `<CANCEL>` on navigation
4. Test with large responses (backup, device list)

### Phase 3: Transaction IDs (Future)

Consider for v2.0 if needed:
- Concurrent command support
- Better error recovery
- Command queuing

---

## Mobile UI Recommendations

### 1. Show Cancellation State

```
┌──────────────────────────────────────────────────────────┐
│  Loading Devices...                              [X]     │
│                                                          │
│  ████████████░░░░░░░░  45%                               │
│                                                          │
│  Tap X or navigate away to cancel                        │
└──────────────────────────────────────────────────────────┘
```

### 2. Navigation Guard

```dart
// Warn user about pending operation
Future<bool> onWillPop() async {
  if (bleService.isTransmissionActive) {
    final shouldLeave = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: Text('Cancel Operation?'),
        content: Text('A command is still in progress. Cancel and leave?'),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context, false),
            child: Text('STAY'),
          ),
          TextButton(
            onPressed: () {
              bleService.cancelCommand();
              Navigator.pop(context, true);
            },
            child: Text('LEAVE'),
          ),
        ],
      ),
    );
    return shouldLeave ?? false;
  }
  return true;
}
```

### 3. Auto-Cancel on Timeout

```dart
Future<Map<String, dynamic>?> sendCommandWithTimeout(
  Map<String, dynamic> command, {
  Duration timeout = const Duration(seconds: 30),
}) async {
  try {
    return await sendCommand(command).timeout(timeout);
  } on TimeoutException {
    await cancelCommand();
    throw BleTimeoutException('Command timed out after ${timeout.inSeconds}s');
  }
}
```

---

## Error Codes

| Code | Description | User Message |
|------|-------------|--------------|
| 350 | Command cancelled | "Operation cancelled" |
| 351 | Previous command still running | "Please wait for current operation" |
| 352 | Cancel failed | "Could not cancel operation" |

---

## Testing Checklist

- [ ] Send large command (backup), cancel mid-stream
- [ ] Navigate away during device list loading
- [ ] Send new command while previous is responding
- [ ] Disconnect during transmission
- [ ] Cancel and immediately send new command
- [ ] Multiple rapid cancellations

---

## Summary

| Level | Complexity | Firmware Change | Effectiveness |
|-------|------------|-----------------|---------------|
| 1. Mobile buffer | Low | No | Medium |
| 2. Cancel command | Medium | Yes | High |
| 3. Transaction ID | High | Yes | Very High |

**Recommendation:** Start with Level 1 immediately, implement Level 2 in v1.0.9.

---

**Document Version:** 1.0 | **Last Updated:** January 2026

**SURIOTA R&D Team** | support@suriota.com
