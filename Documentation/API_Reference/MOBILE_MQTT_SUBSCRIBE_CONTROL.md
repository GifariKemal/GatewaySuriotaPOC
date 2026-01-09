# Mobile App Integration: MQTT Subscribe Control

**Version:** 1.1.0 | **Date:** January 2026 | **Target:** Flutter Mobile Developers

---

## Table of Contents

1. [Overview](#1-overview)
2. [Architecture](#2-architecture)
3. [Getting Started](#3-getting-started)
4. [API Reference](#4-api-reference)
5. [Flutter Implementation](#5-flutter-implementation)
6. [UI Design Guide](#6-ui-design-guide)
7. [State Management](#7-state-management)
8. [Error Handling](#8-error-handling)
9. [Best Practices](#9-best-practices)
10. [Testing](#10-testing)

---

## 1. Overview

### What is MQTT Subscribe Control?

MQTT Subscribe Control memungkinkan mobile app untuk **menulis nilai ke register
Modbus secara remote melalui MQTT**, tanpa koneksi BLE langsung ke gateway.

```
┌─────────────┐      MQTT       ┌─────────────┐     Modbus     ┌─────────────┐
│ Mobile App  │ ──────────────► │   Gateway   │ ─────────────► │   Device    │
│  (Flutter)  │ ◄────────────── │ MGATE-1210  │ ◄───────────── │  (PLC/RTU)  │
└─────────────┘    Response     └─────────────┘    Response    └─────────────┘
```

### Use Cases

| Scenario | Description |
|----------|-------------|
| **Remote Setpoint** | Ubah temperature setpoint dari jarak jauh |
| **Production Control** | Start/stop mesin via mobile app |
| **Parameter Adjustment** | Update konfigurasi PLC tanpa ke lokasi |
| **Multi-Site Management** | Kontrol gateway di berbagai lokasi |

### Key Benefits

- **No BLE Required** - Kontrol dari mana saja via internet
- **Real-time Response** - Feedback langsung via MQTT response topic
- **Secure by Design** - Hanya register yang di-enable yang bisa ditulis
- **Existing Infrastructure** - Gunakan broker MQTT yang sudah ada

---

## 2. Architecture

### Communication Flow

```
Mobile App                    MQTT Broker                    Gateway
    │                             │                             │
    │  1. Get writable registers  │                             │
    │─────────────────────────────│─────────────────────────────│
    │         (via BLE)           │                             │
    │◄────────────────────────────│─────────────────────────────│
    │   List of writable regs     │                             │
    │                             │                             │
    │  2. Subscribe to response   │                             │
    │────────────────────────────►│                             │
    │                             │                             │
    │  3. Publish write command   │                             │
    │────────────────────────────►│  4. Forward to gateway      │
    │                             │────────────────────────────►│
    │                             │                             │
    │                             │  5. Execute Modbus write    │
    │                             │                             │
    │                             │  6. Publish response        │
    │                             │◄────────────────────────────│
    │  7. Receive response        │                             │
    │◄────────────────────────────│                             │
```

### Topic Structure

```
# Write Command (App → Gateway)
suriota/{gateway_id}/write/{device_id}/{topic_suffix}

# Write Response (Gateway → App)
suriota/{gateway_id}/write/{device_id}/{topic_suffix}/response
```

**Example:**

```
Write:    suriota/MGate1210_A3B4C5/write/D7A3F2/temp_setpoint
Response: suriota/MGate1210_A3B4C5/write/D7A3F2/temp_setpoint/response
```

---

## 3. Getting Started

### Prerequisites

1. **Gateway Firmware:** v1.1.0 atau lebih baru
2. **MQTT Broker:** Sudah terkonfigurasi di gateway
3. **Register Config:** `writable: true` dan `mqtt_subscribe.enabled: true`

### Step 1: Enable Subscribe Control di Gateway

Via BLE, update server config:

```json
{
  "action": "update",
  "resource": "server_config",
  "data": {
    "mqtt": {
      "subscribe_control": {
        "enabled": true,
        "response_enabled": true,
        "default_qos": 1
      }
    }
  }
}
```

### Step 2: Configure Register untuk MQTT Subscribe

```json
{
  "action": "update",
  "resource": "register",
  "device_id": "D7A3F2",
  "register_id": "R3C8D1",
  "data": {
    "writable": true,
    "min_value": 0,
    "max_value": 100,
    "mqtt_subscribe": {
      "enabled": true,
      "topic_suffix": "temp_setpoint",
      "qos": 1
    }
  }
}
```

### Step 3: Get Writable Registers List

Gunakan BLE command untuk mendapatkan daftar register yang bisa ditulis:

```json
{
  "action": "read",
  "resource": "writable_registers"
}
```

**Response:**

```json
{
  "status": "ok",
  "total_writable": 5,
  "total_mqtt_enabled": 2,
  "devices": [
    {
      "device_id": "D7A3F2",
      "device_name": "Temperature Controller",
      "registers": [
        {
          "register_id": "R3C8D1",
          "register_name": "temp_setpoint",
          "data_type": "INT16",
          "min_value": 0,
          "max_value": 100,
          "scale": 0.1,
          "offset": 0,
          "unit": "°C",
          "mqtt_subscribe": {
            "enabled": true,
            "topic_suffix": "temp_setpoint",
            "qos": 1
          }
        }
      ]
    }
  ]
}
```

---

## 4. API Reference

### 4.1 Write Command

**Topic:** `suriota/{gateway_id}/write/{device_id}/{topic_suffix}`

**QoS:** 1 (recommended)

**Payload Options:**

```dart
// Option 1: Raw value (simplest)
"25.5"

// Option 2: JSON object
{"value": 25.5}

// Option 3: JSON with UUID (untuk tracking)
{"value": 25.5, "uuid": "abc-123"}
```

### 4.2 Write Response

**Topic:** `suriota/{gateway_id}/write/{device_id}/{topic_suffix}/response`

**Success Response:**

```json
{
  "status": "ok",
  "device_id": "D7A3F2",
  "register_id": "R3C8D1",
  "register_name": "temp_setpoint",
  "written_value": 25.5,
  "raw_value": 255,
  "uuid": "abc-123",
  "timestamp": 1704067200000
}
```

**Error Response:**

```json
{
  "status": "error",
  "device_id": "D7A3F2",
  "register_id": "R3C8D1",
  "error": "Value out of range",
  "error_code": 316,
  "min_value": 0,
  "max_value": 100,
  "received_value": 150,
  "uuid": "abc-123",
  "timestamp": 1704067200000
}
```

### 4.3 Error Codes

| Code | Error | Description |
|------|-------|-------------|
| 310 | MODBUS_TIMEOUT | Device tidak merespons |
| 311 | MODBUS_DEVICE_NOT_FOUND | Device ID tidak ditemukan |
| 312 | MODBUS_REGISTER_NOT_FOUND | Register ID tidak ditemukan |
| 313 | MODBUS_CRC_ERROR | CRC error pada komunikasi |
| 314 | MODBUS_EXCEPTION | Modbus exception dari device |
| 315 | MODBUS_WRITE_NOT_ALLOWED | Register tidak writable |
| 316 | MODBUS_VALUE_OUT_OF_RANGE | Nilai di luar min/max |
| 317 | MODBUS_INVALID_PAYLOAD | Payload tidak valid |

---

## 5. Flutter Implementation

### 5.1 Dependencies

```yaml
# pubspec.yaml
dependencies:
  mqtt_client: ^10.0.0
  provider: ^6.0.0
  uuid: ^4.0.0
```

### 5.2 MQTT Service

```dart
// lib/services/mqtt_write_service.dart

import 'dart:async';
import 'dart:convert';
import 'package:mqtt_client/mqtt_client.dart';
import 'package:mqtt_client/mqtt_server_client.dart';
import 'package:uuid/uuid.dart';

class MqttWriteService {
  static MqttWriteService? _instance;
  MqttServerClient? _client;

  final _responseController = StreamController<WriteResponse>.broadcast();
  Stream<WriteResponse> get responseStream => _responseController.stream;

  // Pending requests for UUID-based tracking
  final Map<String, Completer<WriteResponse>> _pendingRequests = {};

  MqttWriteService._();

  static MqttWriteService get instance {
    _instance ??= MqttWriteService._();
    return _instance!;
  }

  /// Connect to MQTT broker
  Future<bool> connect({
    required String broker,
    required int port,
    String? username,
    String? password,
  }) async {
    try {
      final clientId = 'suriota_mobile_${DateTime.now().millisecondsSinceEpoch}';

      _client = MqttServerClient(broker, clientId)
        ..port = port
        ..keepAlivePeriod = 60
        ..logging(on: false)
        ..onConnected = _onConnected
        ..onDisconnected = _onDisconnected;

      if (username != null && password != null) {
        _client!.connectionMessage = MqttConnectMessage()
            .authenticateAs(username, password)
            .withWillQos(MqttQos.atLeastOnce);
      }

      await _client!.connect();

      // Listen for incoming messages
      _client!.updates!.listen(_onMessage);

      return _client!.connectionStatus!.state == MqttConnectionState.connected;
    } catch (e) {
      print('MQTT connection error: $e');
      return false;
    }
  }

  /// Write register value via MQTT
  Future<WriteResponse> writeRegister({
    required String gatewayId,
    required String deviceId,
    required String topicSuffix,
    required dynamic value,
    int qos = 1,
    Duration timeout = const Duration(seconds: 5),
    bool useUuid = true,
  }) async {
    if (_client == null ||
        _client!.connectionStatus!.state != MqttConnectionState.connected) {
      return WriteResponse.error('Not connected to MQTT broker');
    }

    // Build topics
    final writeTopic = 'suriota/$gatewayId/write/$deviceId/$topicSuffix';
    final responseTopic = '$writeTopic/response';

    // Generate UUID for tracking
    String? uuid;
    if (useUuid) {
      uuid = const Uuid().v4().substring(0, 8);
    }

    // Subscribe to response topic
    _client!.subscribe(responseTopic, MqttQos.values[qos]);

    // Build payload
    String payload;
    if (useUuid) {
      payload = jsonEncode({'value': value, 'uuid': uuid});
    } else {
      payload = value.toString();
    }

    // Create completer for response
    final completer = Completer<WriteResponse>();
    if (uuid != null) {
      _pendingRequests[uuid] = completer;
    }

    // Publish write command
    final builder = MqttClientPayloadBuilder();
    builder.addString(payload);

    _client!.publishMessage(
      writeTopic,
      MqttQos.values[qos],
      builder.payload!,
    );

    // Wait for response with timeout
    try {
      if (uuid != null) {
        final response = await completer.future.timeout(timeout);
        _pendingRequests.remove(uuid);
        return response;
      } else {
        // Without UUID, wait for any response on this topic
        return await _waitForResponse(responseTopic, timeout);
      }
    } on TimeoutException {
      _pendingRequests.remove(uuid);
      return WriteResponse.timeout();
    }
  }

  Future<WriteResponse> _waitForResponse(String topic, Duration timeout) async {
    final completer = Completer<WriteResponse>();

    late StreamSubscription subscription;
    subscription = responseStream.listen((response) {
      if (!completer.isCompleted) {
        completer.complete(response);
        subscription.cancel();
      }
    });

    try {
      return await completer.future.timeout(timeout);
    } on TimeoutException {
      subscription.cancel();
      return WriteResponse.timeout();
    }
  }

  void _onMessage(List<MqttReceivedMessage<MqttMessage>> messages) {
    for (final message in messages) {
      final payload = (message.payload as MqttPublishMessage)
          .payload
          .message;

      final payloadString = utf8.decode(payload);

      try {
        final data = jsonDecode(payloadString) as Map<String, dynamic>;
        final response = WriteResponse.fromJson(data);

        // Check for UUID match
        if (response.uuid != null && _pendingRequests.containsKey(response.uuid)) {
          _pendingRequests[response.uuid]!.complete(response);
        }

        // Broadcast to stream
        _responseController.add(response);
      } catch (e) {
        print('Error parsing MQTT response: $e');
      }
    }
  }

  void _onConnected() {
    print('MQTT Connected');
  }

  void _onDisconnected() {
    print('MQTT Disconnected');
  }

  void disconnect() {
    _client?.disconnect();
  }
}
```

### 5.3 Models

```dart
// lib/models/write_response.dart

class WriteResponse {
  final bool success;
  final String? deviceId;
  final String? registerId;
  final String? registerName;
  final dynamic writtenValue;
  final int? rawValue;
  final String? error;
  final int? errorCode;
  final String? uuid;
  final int? timestamp;
  final double? minValue;
  final double? maxValue;

  WriteResponse({
    required this.success,
    this.deviceId,
    this.registerId,
    this.registerName,
    this.writtenValue,
    this.rawValue,
    this.error,
    this.errorCode,
    this.uuid,
    this.timestamp,
    this.minValue,
    this.maxValue,
  });

  factory WriteResponse.fromJson(Map<String, dynamic> json) {
    return WriteResponse(
      success: json['status'] == 'ok',
      deviceId: json['device_id'],
      registerId: json['register_id'],
      registerName: json['register_name'],
      writtenValue: json['written_value'],
      rawValue: json['raw_value'],
      error: json['error'],
      errorCode: json['error_code'],
      uuid: json['uuid'],
      timestamp: json['timestamp'],
      minValue: json['min_value']?.toDouble(),
      maxValue: json['max_value']?.toDouble(),
    );
  }

  factory WriteResponse.error(String message) {
    return WriteResponse(
      success: false,
      error: message,
    );
  }

  factory WriteResponse.timeout() {
    return WriteResponse(
      success: false,
      error: 'Response timeout',
      errorCode: -1,
    );
  }
}

// lib/models/writable_register.dart

class WritableRegister {
  final String registerId;
  final String registerName;
  final String dataType;
  final double? minValue;
  final double? maxValue;
  final double scale;
  final double offset;
  final String? unit;
  final MqttSubscribeConfig mqttSubscribe;

  WritableRegister({
    required this.registerId,
    required this.registerName,
    required this.dataType,
    this.minValue,
    this.maxValue,
    this.scale = 1.0,
    this.offset = 0.0,
    this.unit,
    required this.mqttSubscribe,
  });

  factory WritableRegister.fromJson(Map<String, dynamic> json) {
    return WritableRegister(
      registerId: json['register_id'],
      registerName: json['register_name'],
      dataType: json['data_type'],
      minValue: json['min_value']?.toDouble(),
      maxValue: json['max_value']?.toDouble(),
      scale: (json['scale'] ?? 1.0).toDouble(),
      offset: (json['offset'] ?? 0.0).toDouble(),
      unit: json['unit'],
      mqttSubscribe: MqttSubscribeConfig.fromJson(json['mqtt_subscribe'] ?? {}),
    );
  }

  /// Get full MQTT topic for this register
  String getTopic(String gatewayId, String deviceId) {
    return 'suriota/$gatewayId/write/$deviceId/${mqttSubscribe.topicSuffix}';
  }
}

class MqttSubscribeConfig {
  final bool enabled;
  final String topicSuffix;
  final int qos;

  MqttSubscribeConfig({
    required this.enabled,
    required this.topicSuffix,
    this.qos = 1,
  });

  factory MqttSubscribeConfig.fromJson(Map<String, dynamic> json) {
    return MqttSubscribeConfig(
      enabled: json['enabled'] ?? false,
      topicSuffix: json['topic_suffix'] ?? '',
      qos: json['qos'] ?? 1,
    );
  }
}
```

### 5.4 Provider

```dart
// lib/providers/mqtt_write_provider.dart

import 'package:flutter/foundation.dart';
import '../services/mqtt_write_service.dart';
import '../models/write_response.dart';
import '../models/writable_register.dart';

enum WriteState { idle, writing, success, error }

class MqttWriteProvider extends ChangeNotifier {
  final MqttWriteService _service = MqttWriteService.instance;

  bool _connected = false;
  WriteState _state = WriteState.idle;
  WriteResponse? _lastResponse;
  String? _errorMessage;

  // Writable registers cache
  List<WritableRegister> _writableRegisters = [];
  Map<String, List<WritableRegister>> _registersByDevice = {};

  bool get connected => _connected;
  WriteState get state => _state;
  WriteResponse? get lastResponse => _lastResponse;
  String? get errorMessage => _errorMessage;
  List<WritableRegister> get writableRegisters => _writableRegisters;

  /// Get registers for specific device
  List<WritableRegister> getRegistersForDevice(String deviceId) {
    return _registersByDevice[deviceId] ?? [];
  }

  /// Connect to MQTT broker
  Future<bool> connect({
    required String broker,
    int port = 1883,
    String? username,
    String? password,
  }) async {
    _connected = await _service.connect(
      broker: broker,
      port: port,
      username: username,
      password: password,
    );
    notifyListeners();
    return _connected;
  }

  /// Set writable registers from BLE response
  void setWritableRegisters(Map<String, dynamic> bleResponse) {
    _writableRegisters.clear();
    _registersByDevice.clear();

    final devices = bleResponse['devices'] as List? ?? [];

    for (final device in devices) {
      final deviceId = device['device_id'] as String;
      final registers = (device['registers'] as List? ?? [])
          .map((r) => WritableRegister.fromJson(r))
          .where((r) => r.mqttSubscribe.enabled)
          .toList();

      _writableRegisters.addAll(registers);
      _registersByDevice[deviceId] = registers;
    }

    notifyListeners();
  }

  /// Write value to register
  Future<WriteResponse> writeValue({
    required String gatewayId,
    required String deviceId,
    required WritableRegister register,
    required dynamic value,
  }) async {
    _state = WriteState.writing;
    _errorMessage = null;
    notifyListeners();

    try {
      // Client-side validation
      if (register.minValue != null && value < register.minValue!) {
        throw ValidationException(
          'Value must be at least ${register.minValue}${register.unit ?? ""}',
        );
      }
      if (register.maxValue != null && value > register.maxValue!) {
        throw ValidationException(
          'Value must be at most ${register.maxValue}${register.unit ?? ""}',
        );
      }

      final response = await _service.writeRegister(
        gatewayId: gatewayId,
        deviceId: deviceId,
        topicSuffix: register.mqttSubscribe.topicSuffix,
        value: value,
        qos: register.mqttSubscribe.qos,
      );

      _lastResponse = response;
      _state = response.success ? WriteState.success : WriteState.error;

      if (!response.success) {
        _errorMessage = response.error;
      }

      notifyListeners();
      return response;

    } on ValidationException catch (e) {
      _state = WriteState.error;
      _errorMessage = e.message;
      notifyListeners();
      return WriteResponse.error(e.message);
    } catch (e) {
      _state = WriteState.error;
      _errorMessage = e.toString();
      notifyListeners();
      return WriteResponse.error(e.toString());
    }
  }

  /// Reset state
  void reset() {
    _state = WriteState.idle;
    _lastResponse = null;
    _errorMessage = null;
    notifyListeners();
  }

  void disconnect() {
    _service.disconnect();
    _connected = false;
    notifyListeners();
  }
}

class ValidationException implements Exception {
  final String message;
  ValidationException(this.message);
}
```

---

## 6. UI Design Guide

### 6.1 Remote Control Screen

```dart
// lib/screens/remote_control_screen.dart

import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

class RemoteControlScreen extends StatefulWidget {
  final String gatewayId;
  final String deviceId;
  final String deviceName;

  const RemoteControlScreen({
    super.key,
    required this.gatewayId,
    required this.deviceId,
    required this.deviceName,
  });

  @override
  State<RemoteControlScreen> createState() => _RemoteControlScreenState();
}

class _RemoteControlScreenState extends State<RemoteControlScreen> {
  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: const Color(0xFF0d1117),
      appBar: AppBar(
        backgroundColor: const Color(0xFF161b22),
        title: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(widget.deviceName),
            Text(
              'Remote Control',
              style: TextStyle(
                fontSize: 12,
                color: Colors.grey[400],
              ),
            ),
          ],
        ),
        actions: [
          _buildConnectionStatus(),
        ],
      ),
      body: Consumer<MqttWriteProvider>(
        builder: (context, provider, _) {
          final registers = provider.getRegistersForDevice(widget.deviceId);

          if (registers.isEmpty) {
            return _buildEmptyState();
          }

          return ListView.builder(
            padding: const EdgeInsets.all(16),
            itemCount: registers.length,
            itemBuilder: (context, index) {
              return _buildRegisterCard(registers[index], provider);
            },
          );
        },
      ),
    );
  }

  Widget _buildConnectionStatus() {
    return Consumer<MqttWriteProvider>(
      builder: (context, provider, _) {
        return Container(
          margin: const EdgeInsets.only(right: 16),
          padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 6),
          decoration: BoxDecoration(
            color: provider.connected
                ? const Color(0xFF3fb950).withOpacity(0.2)
                : const Color(0xFFf85149).withOpacity(0.2),
            borderRadius: BorderRadius.circular(16),
          ),
          child: Row(
            mainAxisSize: MainAxisSize.min,
            children: [
              Container(
                width: 8,
                height: 8,
                decoration: BoxDecoration(
                  shape: BoxShape.circle,
                  color: provider.connected
                      ? const Color(0xFF3fb950)
                      : const Color(0xFFf85149),
                ),
              ),
              const SizedBox(width: 8),
              Text(
                provider.connected ? 'Online' : 'Offline',
                style: TextStyle(
                  color: provider.connected
                      ? const Color(0xFF3fb950)
                      : const Color(0xFFf85149),
                  fontSize: 12,
                  fontWeight: FontWeight.w600,
                ),
              ),
            ],
          ),
        );
      },
    );
  }

  Widget _buildEmptyState() {
    return Center(
      child: Column(
        mainAxisAlignment: MainAxisAlignment.center,
        children: [
          Icon(
            Icons.edit_off,
            size: 64,
            color: Colors.grey[600],
          ),
          const SizedBox(height: 16),
          Text(
            'No Remote Registers',
            style: TextStyle(
              color: Colors.grey[400],
              fontSize: 18,
              fontWeight: FontWeight.w600,
            ),
          ),
          const SizedBox(height: 8),
          Text(
            'Enable mqtt_subscribe on writable registers\nto control them remotely',
            textAlign: TextAlign.center,
            style: TextStyle(
              color: Colors.grey[600],
              fontSize: 14,
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildRegisterCard(WritableRegister register, MqttWriteProvider provider) {
    return RegisterWriteCard(
      gatewayId: widget.gatewayId,
      deviceId: widget.deviceId,
      register: register,
    );
  }
}
```

### 6.2 Register Write Card Widget

```dart
// lib/widgets/register_write_card.dart

class RegisterWriteCard extends StatefulWidget {
  final String gatewayId;
  final String deviceId;
  final WritableRegister register;

  const RegisterWriteCard({
    super.key,
    required this.gatewayId,
    required this.deviceId,
    required this.register,
  });

  @override
  State<RegisterWriteCard> createState() => _RegisterWriteCardState();
}

class _RegisterWriteCardState extends State<RegisterWriteCard> {
  final _controller = TextEditingController();
  bool _isWriting = false;
  String? _error;
  bool _success = false;

  @override
  Widget build(BuildContext context) {
    final reg = widget.register;

    return Container(
      margin: const EdgeInsets.only(bottom: 16),
      decoration: BoxDecoration(
        color: const Color(0xFF161b22),
        borderRadius: BorderRadius.circular(12),
        border: Border.all(
          color: _success
              ? const Color(0xFF3fb950)
              : _error != null
                  ? const Color(0xFFf85149)
                  : const Color(0xFF30363d),
        ),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          // Header
          Container(
            padding: const EdgeInsets.all(16),
            decoration: const BoxDecoration(
              border: Border(
                bottom: BorderSide(color: Color(0xFF30363d)),
              ),
            ),
            child: Row(
              children: [
                // Icon
                Container(
                  padding: const EdgeInsets.all(10),
                  decoration: BoxDecoration(
                    color: const Color(0xFF58a6ff).withOpacity(0.1),
                    borderRadius: BorderRadius.circular(10),
                  ),
                  child: const Icon(
                    Icons.edit_note,
                    color: Color(0xFF58a6ff),
                    size: 24,
                  ),
                ),
                const SizedBox(width: 12),
                // Title
                Expanded(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        reg.registerName,
                        style: const TextStyle(
                          color: Colors.white,
                          fontSize: 16,
                          fontWeight: FontWeight.w600,
                        ),
                      ),
                      const SizedBox(height: 4),
                      Text(
                        'Topic: ${reg.mqttSubscribe.topicSuffix} • QoS ${reg.mqttSubscribe.qos}',
                        style: TextStyle(
                          color: Colors.grey[500],
                          fontSize: 12,
                        ),
                      ),
                    ],
                  ),
                ),
                // Data type badge
                Container(
                  padding: const EdgeInsets.symmetric(
                    horizontal: 10,
                    vertical: 4,
                  ),
                  decoration: BoxDecoration(
                    color: const Color(0xFF21262d),
                    borderRadius: BorderRadius.circular(6),
                  ),
                  child: Text(
                    reg.dataType,
                    style: TextStyle(
                      color: Colors.grey[400],
                      fontSize: 11,
                      fontWeight: FontWeight.w500,
                    ),
                  ),
                ),
              ],
            ),
          ),

          // Input Section
          Padding(
            padding: const EdgeInsets.all(16),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                // Range indicator
                if (reg.minValue != null || reg.maxValue != null)
                  Padding(
                    padding: const EdgeInsets.only(bottom: 12),
                    child: Row(
                      children: [
                        Icon(Icons.info_outline,
                            size: 14, color: Colors.grey[500]),
                        const SizedBox(width: 6),
                        Text(
                          'Range: ${reg.minValue ?? "∞"} - ${reg.maxValue ?? "∞"} ${reg.unit ?? ""}',
                          style: TextStyle(
                            color: Colors.grey[500],
                            fontSize: 12,
                          ),
                        ),
                      ],
                    ),
                  ),

                // Input row
                Row(
                  children: [
                    Expanded(
                      child: TextField(
                        controller: _controller,
                        keyboardType: const TextInputType.numberWithOptions(
                          decimal: true,
                          signed: true,
                        ),
                        style: const TextStyle(
                          color: Colors.white,
                          fontSize: 16,
                        ),
                        decoration: InputDecoration(
                          hintText: 'Enter value',
                          hintStyle: TextStyle(color: Colors.grey[600]),
                          suffixText: reg.unit,
                          suffixStyle: TextStyle(color: Colors.grey[400]),
                          filled: true,
                          fillColor: const Color(0xFF0d1117),
                          contentPadding: const EdgeInsets.symmetric(
                            horizontal: 16,
                            vertical: 14,
                          ),
                          border: OutlineInputBorder(
                            borderRadius: BorderRadius.circular(8),
                            borderSide: const BorderSide(
                              color: Color(0xFF30363d),
                            ),
                          ),
                          enabledBorder: OutlineInputBorder(
                            borderRadius: BorderRadius.circular(8),
                            borderSide: const BorderSide(
                              color: Color(0xFF30363d),
                            ),
                          ),
                          focusedBorder: OutlineInputBorder(
                            borderRadius: BorderRadius.circular(8),
                            borderSide: const BorderSide(
                              color: Color(0xFF58a6ff),
                            ),
                          ),
                          errorText: _error,
                          errorStyle: const TextStyle(
                            color: Color(0xFFf85149),
                          ),
                        ),
                        onChanged: (_) {
                          if (_error != null || _success) {
                            setState(() {
                              _error = null;
                              _success = false;
                            });
                          }
                        },
                      ),
                    ),
                    const SizedBox(width: 12),
                    SizedBox(
                      height: 48,
                      child: ElevatedButton(
                        onPressed: _isWriting ? null : _handleWrite,
                        style: ElevatedButton.styleFrom(
                          backgroundColor: const Color(0xFF3fb950),
                          foregroundColor: Colors.white,
                          disabledBackgroundColor: const Color(0xFF21262d),
                          shape: RoundedRectangleBorder(
                            borderRadius: BorderRadius.circular(8),
                          ),
                          padding: const EdgeInsets.symmetric(horizontal: 24),
                        ),
                        child: _isWriting
                            ? const SizedBox(
                                width: 20,
                                height: 20,
                                child: CircularProgressIndicator(
                                  strokeWidth: 2,
                                  color: Colors.white,
                                ),
                              )
                            : const Text(
                                'Write',
                                style: TextStyle(fontWeight: FontWeight.w600),
                              ),
                      ),
                    ),
                  ],
                ),

                // Success message
                if (_success)
                  Padding(
                    padding: const EdgeInsets.only(top: 12),
                    child: Row(
                      children: [
                        const Icon(
                          Icons.check_circle,
                          size: 16,
                          color: Color(0xFF3fb950),
                        ),
                        const SizedBox(width: 6),
                        Text(
                          'Value written successfully',
                          style: TextStyle(
                            color: const Color(0xFF3fb950),
                            fontSize: 12,
                          ),
                        ),
                      ],
                    ),
                  ),
              ],
            ),
          ),
        ],
      ),
    );
  }

  Future<void> _handleWrite() async {
    final valueStr = _controller.text.trim();
    if (valueStr.isEmpty) {
      setState(() => _error = 'Please enter a value');
      return;
    }

    final value = double.tryParse(valueStr);
    if (value == null) {
      setState(() => _error = 'Invalid number');
      return;
    }

    // Client-side validation
    final reg = widget.register;
    if (reg.minValue != null && value < reg.minValue!) {
      setState(() => _error = 'Value below minimum (${reg.minValue})');
      return;
    }
    if (reg.maxValue != null && value > reg.maxValue!) {
      setState(() => _error = 'Value above maximum (${reg.maxValue})');
      return;
    }

    setState(() {
      _isWriting = true;
      _error = null;
      _success = false;
    });

    try {
      final provider = context.read<MqttWriteProvider>();
      final response = await provider.writeValue(
        gatewayId: widget.gatewayId,
        deviceId: widget.deviceId,
        register: reg,
        value: value,
      );

      setState(() {
        _isWriting = false;
        if (response.success) {
          _success = true;
          _error = null;
        } else {
          _error = response.error ?? 'Write failed';
          _success = false;
        }
      });

      // Auto-hide success after 3 seconds
      if (response.success) {
        Future.delayed(const Duration(seconds: 3), () {
          if (mounted) {
            setState(() => _success = false);
          }
        });
      }
    } catch (e) {
      setState(() {
        _isWriting = false;
        _error = e.toString();
      });
    }
  }

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }
}
```

### 6.3 Toggle Widget (untuk Boolean/Coil)

```dart
// lib/widgets/register_toggle_card.dart

class RegisterToggleCard extends StatefulWidget {
  final String gatewayId;
  final String deviceId;
  final WritableRegister register;
  final bool initialValue;

  const RegisterToggleCard({
    super.key,
    required this.gatewayId,
    required this.deviceId,
    required this.register,
    this.initialValue = false,
  });

  @override
  State<RegisterToggleCard> createState() => _RegisterToggleCardState();
}

class _RegisterToggleCardState extends State<RegisterToggleCard> {
  late bool _value;
  bool _isWriting = false;

  @override
  void initState() {
    super.initState();
    _value = widget.initialValue;
  }

  @override
  Widget build(BuildContext context) {
    return Container(
      margin: const EdgeInsets.only(bottom: 16),
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: const Color(0xFF161b22),
        borderRadius: BorderRadius.circular(12),
        border: Border.all(color: const Color(0xFF30363d)),
      ),
      child: Row(
        children: [
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  widget.register.registerName,
                  style: const TextStyle(
                    color: Colors.white,
                    fontSize: 16,
                    fontWeight: FontWeight.w600,
                  ),
                ),
                const SizedBox(height: 4),
                Text(
                  _value ? 'ON' : 'OFF',
                  style: TextStyle(
                    color: _value
                        ? const Color(0xFF3fb950)
                        : Colors.grey[500],
                    fontSize: 14,
                    fontWeight: FontWeight.w500,
                  ),
                ),
              ],
            ),
          ),
          if (_isWriting)
            const SizedBox(
              width: 24,
              height: 24,
              child: CircularProgressIndicator(strokeWidth: 2),
            )
          else
            Switch(
              value: _value,
              onChanged: _handleToggle,
              activeColor: const Color(0xFF3fb950),
              activeTrackColor: const Color(0xFF3fb950).withOpacity(0.3),
              inactiveThumbColor: Colors.grey[400],
              inactiveTrackColor: Colors.grey[700],
            ),
        ],
      ),
    );
  }

  Future<void> _handleToggle(bool newValue) async {
    final oldValue = _value;

    setState(() {
      _value = newValue;
      _isWriting = true;
    });

    try {
      final provider = context.read<MqttWriteProvider>();
      final response = await provider.writeValue(
        gatewayId: widget.gatewayId,
        deviceId: widget.deviceId,
        register: widget.register,
        value: newValue ? 1 : 0,
      );

      if (!response.success) {
        // Revert on failure
        setState(() => _value = oldValue);
        _showError(response.error ?? 'Write failed');
      }
    } catch (e) {
      setState(() => _value = oldValue);
      _showError(e.toString());
    } finally {
      setState(() => _isWriting = false);
    }
  }

  void _showError(String message) {
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        content: Text(message),
        backgroundColor: const Color(0xFFf85149),
      ),
    );
  }
}
```

### 6.4 Slider Widget (untuk Range Values)

```dart
// lib/widgets/register_slider_card.dart

class RegisterSliderCard extends StatefulWidget {
  final String gatewayId;
  final String deviceId;
  final WritableRegister register;
  final double initialValue;

  const RegisterSliderCard({
    super.key,
    required this.gatewayId,
    required this.deviceId,
    required this.register,
    this.initialValue = 0,
  });

  @override
  State<RegisterSliderCard> createState() => _RegisterSliderCardState();
}

class _RegisterSliderCardState extends State<RegisterSliderCard> {
  late double _value;
  late double _pendingValue;
  bool _isWriting = false;
  Timer? _debounceTimer;

  @override
  void initState() {
    super.initState();
    _value = widget.initialValue;
    _pendingValue = _value;
  }

  @override
  Widget build(BuildContext context) {
    final reg = widget.register;
    final min = reg.minValue ?? 0;
    final max = reg.maxValue ?? 100;

    return Container(
      margin: const EdgeInsets.only(bottom: 16),
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: const Color(0xFF161b22),
        borderRadius: BorderRadius.circular(12),
        border: Border.all(color: const Color(0xFF30363d)),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          // Header
          Row(
            mainAxisAlignment: MainAxisAlignment.spaceBetween,
            children: [
              Text(
                reg.registerName,
                style: const TextStyle(
                  color: Colors.white,
                  fontSize: 16,
                  fontWeight: FontWeight.w600,
                ),
              ),
              Row(
                children: [
                  Text(
                    _pendingValue.toStringAsFixed(1),
                    style: const TextStyle(
                      color: Color(0xFF58a6ff),
                      fontSize: 20,
                      fontWeight: FontWeight.bold,
                    ),
                  ),
                  if (reg.unit != null)
                    Text(
                      ' ${reg.unit}',
                      style: TextStyle(
                        color: Colors.grey[400],
                        fontSize: 14,
                      ),
                    ),
                  if (_isWriting) ...[
                    const SizedBox(width: 8),
                    const SizedBox(
                      width: 16,
                      height: 16,
                      child: CircularProgressIndicator(strokeWidth: 2),
                    ),
                  ],
                ],
              ),
            ],
          ),

          const SizedBox(height: 16),

          // Slider
          SliderTheme(
            data: SliderThemeData(
              activeTrackColor: const Color(0xFF58a6ff),
              inactiveTrackColor: const Color(0xFF30363d),
              thumbColor: const Color(0xFF58a6ff),
              overlayColor: const Color(0xFF58a6ff).withOpacity(0.2),
              trackHeight: 6,
            ),
            child: Slider(
              value: _pendingValue.clamp(min, max),
              min: min,
              max: max,
              onChanged: (value) {
                setState(() => _pendingValue = value);
                _debounceWrite(value);
              },
            ),
          ),

          // Min/Max labels
          Row(
            mainAxisAlignment: MainAxisAlignment.spaceBetween,
            children: [
              Text(
                '${min.toStringAsFixed(0)} ${reg.unit ?? ""}',
                style: TextStyle(color: Colors.grey[500], fontSize: 12),
              ),
              Text(
                '${max.toStringAsFixed(0)} ${reg.unit ?? ""}',
                style: TextStyle(color: Colors.grey[500], fontSize: 12),
              ),
            ],
          ),
        ],
      ),
    );
  }

  void _debounceWrite(double value) {
    _debounceTimer?.cancel();
    _debounceTimer = Timer(const Duration(milliseconds: 500), () {
      _writeValue(value);
    });
  }

  Future<void> _writeValue(double value) async {
    setState(() => _isWriting = true);

    try {
      final provider = context.read<MqttWriteProvider>();
      final response = await provider.writeValue(
        gatewayId: widget.gatewayId,
        deviceId: widget.deviceId,
        register: widget.register,
        value: value,
      );

      if (response.success) {
        setState(() => _value = value);
      } else {
        // Revert to last successful value
        setState(() => _pendingValue = _value);
        _showError(response.error ?? 'Write failed');
      }
    } catch (e) {
      setState(() => _pendingValue = _value);
      _showError(e.toString());
    } finally {
      setState(() => _isWriting = false);
    }
  }

  void _showError(String message) {
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        content: Text(message),
        backgroundColor: const Color(0xFFf85149),
      ),
    );
  }

  @override
  void dispose() {
    _debounceTimer?.cancel();
    super.dispose();
  }
}
```

---

## 7. State Management

### 7.1 Provider Setup

```dart
// lib/main.dart

void main() {
  runApp(
    MultiProvider(
      providers: [
        ChangeNotifierProvider(create: (_) => MqttWriteProvider()),
        // ... other providers
      ],
      child: const MyApp(),
    ),
  );
}
```

### 7.2 Initialize pada App Start

```dart
// lib/screens/home_screen.dart

class _HomeScreenState extends State<HomeScreen> {
  @override
  void initState() {
    super.initState();
    _initMqtt();
  }

  Future<void> _initMqtt() async {
    final provider = context.read<MqttWriteProvider>();

    // Connect to MQTT broker
    await provider.connect(
      broker: 'broker.hivemq.com',  // atau broker internal
      port: 1883,
    );

    // Load writable registers via BLE
    final bleResponse = await BleService.instance.sendCommand({
      'action': 'read',
      'resource': 'writable_registers',
    });

    provider.setWritableRegisters(bleResponse);
  }
}
```

---

## 8. Error Handling

### 8.1 Error Display Widget

```dart
class WriteErrorWidget extends StatelessWidget {
  final int? errorCode;
  final String message;
  final VoidCallback? onRetry;

  const WriteErrorWidget({
    super.key,
    this.errorCode,
    required this.message,
    this.onRetry,
  });

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: const Color(0xFFf85149).withOpacity(0.1),
        borderRadius: BorderRadius.circular(8),
        border: Border.all(color: const Color(0xFFf85149).withOpacity(0.3)),
      ),
      child: Row(
        children: [
          const Icon(Icons.error_outline, color: Color(0xFFf85149)),
          const SizedBox(width: 12),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                if (errorCode != null)
                  Text(
                    'Error ${errorCode}',
                    style: const TextStyle(
                      color: Color(0xFFf85149),
                      fontWeight: FontWeight.bold,
                    ),
                  ),
                Text(
                  message,
                  style: TextStyle(color: Colors.grey[300]),
                ),
              ],
            ),
          ),
          if (onRetry != null)
            TextButton(
              onPressed: onRetry,
              child: const Text('Retry'),
            ),
        ],
      ),
    );
  }
}
```

### 8.2 Error Code Mapping

```dart
// lib/utils/error_messages.dart

class MqttWriteErrors {
  static String getMessage(int? errorCode) {
    switch (errorCode) {
      case 310:
        return 'Device tidak merespons. Periksa koneksi Modbus.';
      case 311:
        return 'Device tidak ditemukan.';
      case 312:
        return 'Register tidak ditemukan.';
      case 314:
        return 'Device menolak perintah.';
      case 315:
        return 'Register ini tidak dapat ditulis.';
      case 316:
        return 'Nilai di luar batas yang diizinkan.';
      case 317:
        return 'Format payload tidak valid.';
      case -1:
        return 'Timeout menunggu respons. Gateway mungkin offline.';
      default:
        return 'Terjadi kesalahan. Silakan coba lagi.';
    }
  }
}
```

---

## 9. Best Practices

### 9.1 Connection Management

```dart
// Always check connection before write
if (!provider.connected) {
  await provider.connect(broker: brokerUrl);
}

// Reconnect on disconnect
provider.addListener(() {
  if (!provider.connected && shouldReconnect) {
    provider.connect(broker: brokerUrl);
  }
});
```

### 9.2 Client-Side Validation

```dart
// Always validate before sending
bool validateValue(WritableRegister reg, double value) {
  if (reg.minValue != null && value < reg.minValue!) {
    return false;
  }
  if (reg.maxValue != null && value > reg.maxValue!) {
    return false;
  }
  return true;
}
```

### 9.3 Optimistic UI

```dart
// Show immediate feedback, revert on failure
void handleToggle(bool newValue) {
  final oldValue = currentValue;

  // Update UI immediately
  setState(() => currentValue = newValue);

  // Send to server
  writeValue(newValue).then((response) {
    if (!response.success) {
      // Revert on failure
      setState(() => currentValue = oldValue);
      showError(response.error);
    }
  });
}
```

### 9.4 Debounce Slider Changes

```dart
// Don't send every slider change
Timer? _debounceTimer;

void onSliderChanged(double value) {
  setState(() => displayValue = value);

  _debounceTimer?.cancel();
  _debounceTimer = Timer(
    const Duration(milliseconds: 500),
    () => writeValue(value),
  );
}
```

---

## 10. Testing

### 10.1 Python GUI Tester

Gunakan GUI tester yang tersedia di:

```
Testing/MQTT_Subscribe_Control/mqtt_subscribe_control_gui.py
```

### 10.2 Test Scenarios

| Scenario | Steps | Expected |
|----------|-------|----------|
| Basic Write | Send valid value | Response status: ok |
| Out of Range | Send value > max | Error code: 316 |
| Offline Device | Write when device off | Error code: 310 |
| Non-writable | Write to read-only | Error code: 315 |
| Timeout | Gateway offline | Response timeout |

### 10.3 Integration Test

```dart
// test/mqtt_write_test.dart

void main() {
  test('Write register success', () async {
    final service = MqttWriteService.instance;

    await service.connect(broker: 'test.mosquitto.org', port: 1883);

    final response = await service.writeRegister(
      gatewayId: 'TEST_GATEWAY',
      deviceId: 'TEST_DEVICE',
      topicSuffix: 'test_register',
      value: 25.5,
    );

    expect(response.success, isTrue);
    expect(response.writtenValue, equals(25.5));
  });
}
```

---

## Quick Reference Card

```
┌─────────────────────────────────────────────────────────────────┐
│                 MQTT SUBSCRIBE CONTROL CHEATSHEET               │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  TOPIC FORMAT                                                   │
│  Write:    suriota/{gateway}/write/{device}/{topic_suffix}      │
│  Response: suriota/{gateway}/write/{device}/{topic_suffix}/resp │
│                                                                 │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  PAYLOAD OPTIONS                                                │
│  Raw:      "25.5"                                               │
│  JSON:     {"value": 25.5}                                      │
│  +UUID:    {"value": 25.5, "uuid": "abc-123"}                   │
│                                                                 │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  SUCCESS RESPONSE                                               │
│  {                                                              │
│    "status": "ok",                                              │
│    "written_value": 25.5,                                       │
│    "raw_value": 255                                             │
│  }                                                              │
│                                                                 │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ERROR CODES                                                    │
│  310 = Timeout       315 = Not writable                         │
│  311 = Device N/F    316 = Out of range                         │
│  312 = Register N/F  317 = Invalid payload                      │
│                                                                 │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  BLE COMMAND: Get Writable Registers                            │
│  {"action": "read", "resource": "writable_registers"}           │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

**SURIOTA R&D Team** | support@suriota.com

**Related Documentation:**

- `API.md` - Full BLE CRUD API
- `MOBILE_WRITE_FEATURE.md` - BLE Write Feature
- `MQTT_SUBSCRIBE_CONTROL.md` - Technical Specification
