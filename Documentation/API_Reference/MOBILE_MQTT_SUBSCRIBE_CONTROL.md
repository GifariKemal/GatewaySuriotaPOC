# Mobile App Integration: MQTT Subscribe Control v1.2.0

**Version:** 1.2.0 | **Date:** January 2026 | **Target:** Flutter Mobile Developers

---

## Table of Contents

1. [Overview](#1-overview)
2. [Architecture](#2-architecture)
3. [Configuration API](#3-configuration-api)
4. [MQTT Protocol](#4-mqtt-protocol)
5. [Flutter Implementation](#5-flutter-implementation)
6. [UI Design Guide](#6-ui-design-guide)
7. [Error Handling](#7-error-handling)
8. [Testing](#8-testing)

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

### v1.2.0 Changes (Topic-Centric)

| Aspect | v1.1.0 (Old) | v1.2.0 (New) |
|--------|--------------|--------------|
| Config Location | Per-register `mqtt_subscribe` | `server_config.custom_subscribe_mode` |
| Topic Format | Auto: `suriota/{gw}/write/{dev}/{suffix}` | User-defined custom topic |
| 1 Topic Controls | 1 register | 1 or N registers |
| Payload | `{"value": X}` only | `{"value": X}` or `{"RegA": X, "RegB": Y}` |

### Writable Registers

**Hanya register dengan Function Code berikut yang bisa di-write:**

| FC | Type | Writable | Write FC |
|----|------|----------|----------|
| FC01 | Coil | ✅ Yes | FC05/FC15 |
| FC02 | Discrete Input | ❌ No | - |
| FC03 | Holding Register | ✅ Yes | FC06/FC16 |
| FC04 | Input Register | ❌ No | - |

---

## 2. Architecture

### Topic-Centric Approach

```
┌─────────────────────────────────────────────────────────────────────┐
│  Mobile App Configuration (via BLE)                                 │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  server_config.mqtt_config.custom_subscribe_mode.subscriptions[]    │
│                                                                     │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │ Subscription 1:                                             │   │
│  │   topic: "suriota/gw001/write/temperature"                  │   │
│  │   registers: [{device_id, register_id}]  ← 1 register       │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                     │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │ Subscription 2:                                             │   │
│  │   topic: "suriota/gw001/write/valves"                       │   │
│  │   registers: [{...}, {...}]  ← Multiple registers           │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────────┐
│  Gateway subscribes to configured topics                            │
│  On message → Parse payload → Write to Modbus → Publish response    │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 3. Configuration API

### 3.1 Read Current Server Config

**BLE Request:**
```json
{
  "op": "read",
  "type": "server_config"
}
```

**Response (relevant section):**
```json
{
  "status": "ok",
  "server_config": {
    "mqtt_config": {
      "enabled": true,
      "broker_address": "192.168.1.100",
      "broker_port": 1883,
      "topic_mode": "custom_subscribe",
      "custom_subscribe_mode": {
        "enabled": true,
        "subscriptions": [...]
      }
    }
  }
}
```

### 3.2 Update Server Config with Subscriptions

**BLE Request:**
```json
{
  "op": "update",
  "type": "server_config",
  "config": {
    "mqtt_config": {
      "topic_mode": "custom_subscribe",
      "custom_subscribe_mode": {
        "enabled": true,
        "subscriptions": [
          {
            "topic": "suriota/gw001/write/temp_setpoint",
            "qos": 1,
            "response_topic": "suriota/gw001/response/temp_setpoint",
            "registers": [
              {"device_id": "HVAC01", "register_id": "TempSetpoint"}
            ]
          },
          {
            "topic": "suriota/gw001/write/valves",
            "qos": 2,
            "response_topic": "suriota/gw001/response/valves",
            "registers": [
              {"device_id": "Valve01", "register_id": "ValveA"},
              {"device_id": "Valve01", "register_id": "ValveB"}
            ]
          }
        ]
      }
    }
  }
}
```

**Response:**
```json
{
  "status": "ok",
  "message": "Server config updated successfully"
}
```

### 3.3 Configuration Schema

#### Subscription Object

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `topic` | string | Yes | MQTT topic to subscribe |
| `qos` | integer | No | QoS level 0, 1, or 2 (default: 1) |
| `response_topic` | string | No | Response topic (default: `{topic}/response`) |
| `registers` | array | Yes | Array of register references |

#### Register Reference

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `device_id` | string | Yes | Modbus device ID |
| `register_id` | string | Yes | Register ID to write |

---

## 4. MQTT Protocol

### 4.1 Single Register Write

**Config:**
```json
{
  "topic": "suriota/gw001/write/temperature",
  "registers": [
    {"device_id": "HVAC01", "register_id": "TempSetpoint"}
  ]
}
```

**Publish to topic:**
```json
{"value": 25.5}
```

**Response:**
```json
{
  "status": "ok",
  "topic": "suriota/gw001/write/temperature",
  "results": [
    {
      "device_id": "HVAC01",
      "register_id": "TempSetpoint",
      "status": "ok",
      "written_value": 25.5,
      "raw_value": 255
    }
  ],
  "timestamp": 1736697600000
}
```

### 4.2 Multi-Register Write

**Config:**
```json
{
  "topic": "suriota/gw001/write/valves",
  "registers": [
    {"device_id": "Valve01", "register_id": "ValveA"},
    {"device_id": "Valve01", "register_id": "ValveB"}
  ]
}
```

**Publish to topic:**
```json
{
  "ValveA": 1,
  "ValveB": 0
}
```

**Response:**
```json
{
  "status": "ok",
  "topic": "suriota/gw001/write/valves",
  "results": [
    {"device_id": "Valve01", "register_id": "ValveA", "status": "ok", "written_value": 1},
    {"device_id": "Valve01", "register_id": "ValveB", "status": "ok", "written_value": 0}
  ],
  "timestamp": 1736697600000
}
```

### 4.3 Payload Rules

| Scenario | Payload | Result |
|----------|---------|--------|
| 1 register subscription | `{"value": 25.5}` | ✅ Write 25.5 |
| N registers subscription | `{"RegA": 1, "RegB": 0}` | ✅ Write each |
| N registers + `{"value": X}` | `{"value": 25.5}` | ❌ ERROR |

### 4.4 Error Response

```json
{
  "status": "error",
  "topic": "suriota/gw001/write/valves",
  "error": "Multiple registers require explicit values per register_id",
  "error_code": 400,
  "timestamp": 1736697600000
}
```

---

## 5. Flutter Implementation

### 5.1 Models

```dart
// lib/models/mqtt_subscription.dart

class MqttSubscription {
  final String topic;
  final int qos;
  final String responseTopic;
  final List<RegisterReference> registers;

  MqttSubscription({
    required this.topic,
    this.qos = 1,
    String? responseTopic,
    required this.registers,
  }) : responseTopic = responseTopic ?? '$topic/response';

  Map<String, dynamic> toJson() => {
    'topic': topic,
    'qos': qos,
    'response_topic': responseTopic,
    'registers': registers.map((r) => r.toJson()).toList(),
  };

  factory MqttSubscription.fromJson(Map<String, dynamic> json) {
    return MqttSubscription(
      topic: json['topic'],
      qos: json['qos'] ?? 1,
      responseTopic: json['response_topic'],
      registers: (json['registers'] as List)
          .map((r) => RegisterReference.fromJson(r))
          .toList(),
    );
  }
}

class RegisterReference {
  final String deviceId;
  final String registerId;

  RegisterReference({
    required this.deviceId,
    required this.registerId,
  });

  Map<String, dynamic> toJson() => {
    'device_id': deviceId,
    'register_id': registerId,
  };

  factory RegisterReference.fromJson(Map<String, dynamic> json) {
    return RegisterReference(
      deviceId: json['device_id'],
      registerId: json['register_id'],
    );
  }
}

class CustomSubscribeMode {
  final bool enabled;
  final List<MqttSubscription> subscriptions;

  CustomSubscribeMode({
    required this.enabled,
    required this.subscriptions,
  });

  Map<String, dynamic> toJson() => {
    'enabled': enabled,
    'subscriptions': subscriptions.map((s) => s.toJson()).toList(),
  };

  factory CustomSubscribeMode.fromJson(Map<String, dynamic> json) {
    return CustomSubscribeMode(
      enabled: json['enabled'] ?? false,
      subscriptions: (json['subscriptions'] as List? ?? [])
          .map((s) => MqttSubscription.fromJson(s))
          .toList(),
    );
  }
}
```

### 5.2 BLE Service - Update Config

```dart
// lib/services/ble_config_service.dart

class BleConfigService {
  final BleManager _bleManager;

  Future<void> updateSubscriptions(List<MqttSubscription> subscriptions) async {
    final command = {
      'op': 'update',
      'type': 'server_config',
      'config': {
        'mqtt_config': {
          'topic_mode': 'custom_subscribe',
          'custom_subscribe_mode': {
            'enabled': true,
            'subscriptions': subscriptions.map((s) => s.toJson()).toList(),
          },
        },
      },
    };

    final response = await _bleManager.sendCommand(command);
    if (response['status'] != 'ok') {
      throw Exception(response['error'] ?? 'Failed to update subscriptions');
    }
  }

  Future<CustomSubscribeMode> getSubscribeConfig() async {
    final response = await _bleManager.sendCommand({
      'op': 'read',
      'type': 'server_config',
    });

    if (response['status'] == 'ok') {
      final mqttConfig = response['server_config']['mqtt_config'];
      return CustomSubscribeMode.fromJson(
        mqttConfig['custom_subscribe_mode'] ?? {},
      );
    }
    throw Exception('Failed to read config');
  }
}
```

### 5.3 MQTT Write Service

```dart
// lib/services/mqtt_write_service.dart

import 'package:mqtt_client/mqtt_client.dart';
import 'package:mqtt_client/mqtt_server_client.dart';

class MqttWriteService {
  final MqttServerClient _client;
  final Map<String, StreamController<WriteResponse>> _responseStreams = {};

  MqttWriteService(String broker, int port, String clientId)
      : _client = MqttServerClient(broker, clientId) {
    _client.port = port;
    _client.keepAlivePeriod = 60;
    _client.onConnected = _onConnected;
    _client.onDisconnected = _onDisconnected;
  }

  Future<void> connect({String? username, String? password}) async {
    final connMessage = MqttConnectMessage()
        .withClientIdentifier(_client.clientIdentifier)
        .startClean()
        .withWillQos(MqttQos.atLeastOnce);

    if (username != null && password != null) {
      connMessage.authenticateAs(username, password);
    }

    _client.connectionMessage = connMessage;
    await _client.connect();
  }

  /// Subscribe to response topic
  void subscribeToResponse(String responseTopic, Function(WriteResponse) onResponse) {
    _client.subscribe(responseTopic, MqttQos.atLeastOnce);

    _client.updates!.listen((List<MqttReceivedMessage<MqttMessage>> messages) {
      for (var message in messages) {
        if (message.topic == responseTopic) {
          final payload = MqttPublishPayload.bytesToStringAsString(
            (message.payload as MqttPublishMessage).payload.message,
          );
          final response = WriteResponse.fromJson(jsonDecode(payload));
          onResponse(response);
        }
      }
    });
  }

  /// Write single value to single-register subscription
  Future<void> writeSingleValue({
    required String topic,
    required double value,
    int qos = 1,
  }) async {
    final payload = jsonEncode({'value': value});
    _publish(topic, payload, qos);
  }

  /// Write multiple values to multi-register subscription
  Future<void> writeMultipleValues({
    required String topic,
    required Map<String, dynamic> values,
    int qos = 1,
  }) async {
    final payload = jsonEncode(values);
    _publish(topic, payload, qos);
  }

  void _publish(String topic, String payload, int qos) {
    final builder = MqttClientPayloadBuilder();
    builder.addString(payload);

    _client.publishMessage(
      topic,
      qos == 0 ? MqttQos.atMostOnce :
      qos == 1 ? MqttQos.atLeastOnce : MqttQos.exactlyOnce,
      builder.payload!,
    );
  }

  void disconnect() {
    _client.disconnect();
  }
}

class WriteResponse {
  final String status;
  final String topic;
  final List<WriteResult> results;
  final String? error;
  final int? errorCode;
  final int timestamp;

  WriteResponse({
    required this.status,
    required this.topic,
    this.results = const [],
    this.error,
    this.errorCode,
    required this.timestamp,
  });

  bool get isSuccess => status == 'ok';
  bool get isPartial => status == 'partial';
  bool get isError => status == 'error';

  factory WriteResponse.fromJson(Map<String, dynamic> json) {
    return WriteResponse(
      status: json['status'],
      topic: json['topic'],
      results: (json['results'] as List? ?? [])
          .map((r) => WriteResult.fromJson(r))
          .toList(),
      error: json['error'],
      errorCode: json['error_code'],
      timestamp: json['timestamp'],
    );
  }
}

class WriteResult {
  final String deviceId;
  final String registerId;
  final String status;
  final double? writtenValue;
  final String? error;

  WriteResult({
    required this.deviceId,
    required this.registerId,
    required this.status,
    this.writtenValue,
    this.error,
  });

  bool get isSuccess => status == 'ok';

  factory WriteResult.fromJson(Map<String, dynamic> json) {
    return WriteResult(
      deviceId: json['device_id'],
      registerId: json['register_id'],
      status: json['status'],
      writtenValue: json['written_value']?.toDouble(),
      error: json['error'],
    );
  }
}
```

### 5.4 State Management (Riverpod)

```dart
// lib/providers/mqtt_subscribe_provider.dart

import 'package:flutter_riverpod/flutter_riverpod.dart';

final mqttWriteServiceProvider = Provider<MqttWriteService>((ref) {
  final config = ref.watch(mqttConfigProvider);
  return MqttWriteService(
    config.brokerAddress,
    config.brokerPort,
    config.clientId,
  );
});

final subscriptionsProvider = StateNotifierProvider<SubscriptionsNotifier, List<MqttSubscription>>((ref) {
  return SubscriptionsNotifier(ref.read(bleConfigServiceProvider));
});

class SubscriptionsNotifier extends StateNotifier<List<MqttSubscription>> {
  final BleConfigService _bleService;

  SubscriptionsNotifier(this._bleService) : super([]);

  Future<void> loadSubscriptions() async {
    final config = await _bleService.getSubscribeConfig();
    state = config.subscriptions;
  }

  Future<void> addSubscription(MqttSubscription subscription) async {
    final updated = [...state, subscription];
    await _bleService.updateSubscriptions(updated);
    state = updated;
  }

  Future<void> removeSubscription(String topic) async {
    final updated = state.where((s) => s.topic != topic).toList();
    await _bleService.updateSubscriptions(updated);
    state = updated;
  }

  Future<void> updateSubscription(MqttSubscription subscription) async {
    final updated = state.map((s) =>
      s.topic == subscription.topic ? subscription : s
    ).toList();
    await _bleService.updateSubscriptions(updated);
    state = updated;
  }
}
```

---

## 6. UI Design Guide

### 6.1 Subscription List Screen

```dart
// lib/screens/mqtt_subscriptions_screen.dart

class MqttSubscriptionsScreen extends ConsumerWidget {
  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final subscriptions = ref.watch(subscriptionsProvider);

    return Scaffold(
      appBar: AppBar(
        title: Text('MQTT Write Subscriptions'),
        actions: [
          IconButton(
            icon: Icon(Icons.add),
            onPressed: () => _showAddSubscriptionDialog(context, ref),
          ),
        ],
      ),
      body: ListView.builder(
        itemCount: subscriptions.length,
        itemBuilder: (context, index) {
          final sub = subscriptions[index];
          return SubscriptionCard(
            subscription: sub,
            onTap: () => _navigateToControl(context, sub),
            onDelete: () => ref.read(subscriptionsProvider.notifier)
                .removeSubscription(sub.topic),
          );
        },
      ),
    );
  }
}

class SubscriptionCard extends StatelessWidget {
  final MqttSubscription subscription;
  final VoidCallback onTap;
  final VoidCallback onDelete;

  @override
  Widget build(BuildContext context) {
    return Card(
      margin: EdgeInsets.all(8),
      child: ListTile(
        leading: Icon(
          subscription.registers.length > 1
            ? Icons.layers
            : Icons.edit_note,
          color: Theme.of(context).primaryColor,
        ),
        title: Text(subscription.topic),
        subtitle: Text(
          '${subscription.registers.length} register(s) • QoS ${subscription.qos}',
        ),
        trailing: Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            IconButton(
              icon: Icon(Icons.delete, color: Colors.red),
              onPressed: onDelete,
            ),
            Icon(Icons.chevron_right),
          ],
        ),
        onTap: onTap,
      ),
    );
  }
}
```

### 6.2 Add Subscription Dialog

```dart
class AddSubscriptionDialog extends ConsumerStatefulWidget {
  @override
  _AddSubscriptionDialogState createState() => _AddSubscriptionDialogState();
}

class _AddSubscriptionDialogState extends ConsumerState<AddSubscriptionDialog> {
  final _topicController = TextEditingController();
  int _qos = 1;
  List<RegisterReference> _selectedRegisters = [];

  @override
  Widget build(BuildContext context) {
    final writableRegisters = ref.watch(writableRegistersProvider);

    return AlertDialog(
      title: Text('Add Subscription'),
      content: SingleChildScrollView(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            // Topic input
            TextField(
              controller: _topicController,
              decoration: InputDecoration(
                labelText: 'MQTT Topic',
                hintText: 'suriota/gw001/write/temperature',
              ),
            ),
            SizedBox(height: 16),

            // QoS selector
            Text('QoS Level'),
            SegmentedButton<int>(
              segments: [
                ButtonSegment(value: 0, label: Text('0')),
                ButtonSegment(value: 1, label: Text('1')),
                ButtonSegment(value: 2, label: Text('2')),
              ],
              selected: {_qos},
              onSelectionChanged: (Set<int> selected) {
                setState(() => _qos = selected.first);
              },
            ),
            SizedBox(height: 16),

            // Register selection
            Text('Select Registers (FC01/FC03 only)'),
            ...writableRegisters.map((reg) => CheckboxListTile(
              title: Text(reg.registerName),
              subtitle: Text('${reg.deviceName} • ${reg.registerId}'),
              value: _selectedRegisters.any(
                (r) => r.registerId == reg.registerId && r.deviceId == reg.deviceId
              ),
              onChanged: (selected) {
                setState(() {
                  if (selected == true) {
                    _selectedRegisters.add(RegisterReference(
                      deviceId: reg.deviceId,
                      registerId: reg.registerId,
                    ));
                  } else {
                    _selectedRegisters.removeWhere(
                      (r) => r.registerId == reg.registerId
                    );
                  }
                });
              },
            )).toList(),
          ],
        ),
      ),
      actions: [
        TextButton(
          onPressed: () => Navigator.pop(context),
          child: Text('Cancel'),
        ),
        ElevatedButton(
          onPressed: _selectedRegisters.isEmpty ? null : _addSubscription,
          child: Text('Add'),
        ),
      ],
    );
  }

  void _addSubscription() {
    final subscription = MqttSubscription(
      topic: _topicController.text,
      qos: _qos,
      registers: _selectedRegisters,
    );
    ref.read(subscriptionsProvider.notifier).addSubscription(subscription);
    Navigator.pop(context);
  }
}
```

### 6.3 Control Screen - Single Register

```dart
class SingleRegisterControlScreen extends ConsumerWidget {
  final MqttSubscription subscription;

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final register = subscription.registers.first;

    return Scaffold(
      appBar: AppBar(title: Text(subscription.topic)),
      body: Padding(
        padding: EdgeInsets.all(16),
        child: Column(
          children: [
            // Register info
            Card(
              child: ListTile(
                title: Text(register.registerId),
                subtitle: Text('Device: ${register.deviceId}'),
              ),
            ),
            SizedBox(height: 24),

            // Value input
            RegisterWriteCard(
              subscription: subscription,
              onWrite: (value) async {
                final mqttService = ref.read(mqttWriteServiceProvider);
                await mqttService.writeSingleValue(
                  topic: subscription.topic,
                  value: value,
                );
              },
            ),
          ],
        ),
      ),
    );
  }
}

class RegisterWriteCard extends StatefulWidget {
  final MqttSubscription subscription;
  final Future<void> Function(double value) onWrite;

  @override
  _RegisterWriteCardState createState() => _RegisterWriteCardState();
}

class _RegisterWriteCardState extends State<RegisterWriteCard> {
  final _controller = TextEditingController();
  bool _isLoading = false;

  @override
  Widget build(BuildContext context) {
    return Card(
      child: Padding(
        padding: EdgeInsets.all(16),
        child: Column(
          children: [
            TextField(
              controller: _controller,
              keyboardType: TextInputType.numberWithOptions(decimal: true),
              decoration: InputDecoration(
                labelText: 'Value',
                border: OutlineInputBorder(),
              ),
            ),
            SizedBox(height: 16),
            SizedBox(
              width: double.infinity,
              child: ElevatedButton(
                onPressed: _isLoading ? null : _write,
                child: _isLoading
                  ? CircularProgressIndicator()
                  : Text('Write'),
              ),
            ),
          ],
        ),
      ),
    );
  }

  Future<void> _write() async {
    final value = double.tryParse(_controller.text);
    if (value == null) return;

    setState(() => _isLoading = true);
    try {
      await widget.onWrite(value);
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Write successful')),
      );
    } catch (e) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Write failed: $e')),
      );
    } finally {
      setState(() => _isLoading = false);
    }
  }
}
```

### 6.4 Control Screen - Multi Register

```dart
class MultiRegisterControlScreen extends ConsumerStatefulWidget {
  final MqttSubscription subscription;

  @override
  _MultiRegisterControlScreenState createState() => _MultiRegisterControlScreenState();
}

class _MultiRegisterControlScreenState extends ConsumerState<MultiRegisterControlScreen> {
  final Map<String, TextEditingController> _controllers = {};
  bool _isLoading = false;

  @override
  void initState() {
    super.initState();
    for (var reg in widget.subscription.registers) {
      _controllers[reg.registerId] = TextEditingController();
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: Text(widget.subscription.topic)),
      body: SingleChildScrollView(
        padding: EdgeInsets.all(16),
        child: Column(
          children: [
            // Register inputs
            ...widget.subscription.registers.map((reg) => Card(
              margin: EdgeInsets.only(bottom: 12),
              child: Padding(
                padding: EdgeInsets.all(16),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(
                      reg.registerId,
                      style: Theme.of(context).textTheme.titleMedium,
                    ),
                    Text(
                      'Device: ${reg.deviceId}',
                      style: Theme.of(context).textTheme.bodySmall,
                    ),
                    SizedBox(height: 8),
                    TextField(
                      controller: _controllers[reg.registerId],
                      keyboardType: TextInputType.numberWithOptions(decimal: true),
                      decoration: InputDecoration(
                        labelText: 'Value',
                        border: OutlineInputBorder(),
                      ),
                    ),
                  ],
                ),
              ),
            )).toList(),

            SizedBox(height: 16),

            // Write all button
            SizedBox(
              width: double.infinity,
              height: 48,
              child: ElevatedButton(
                onPressed: _isLoading ? null : _writeAll,
                child: _isLoading
                  ? CircularProgressIndicator(color: Colors.white)
                  : Text('Write All'),
              ),
            ),
          ],
        ),
      ),
    );
  }

  Future<void> _writeAll() async {
    final values = <String, dynamic>{};
    for (var entry in _controllers.entries) {
      final value = double.tryParse(entry.value.text);
      if (value != null) {
        values[entry.key] = value;
      }
    }

    if (values.isEmpty) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Please enter at least one value')),
      );
      return;
    }

    setState(() => _isLoading = true);
    try {
      final mqttService = ref.read(mqttWriteServiceProvider);
      await mqttService.writeMultipleValues(
        topic: widget.subscription.topic,
        values: values,
      );
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Write successful')),
      );
    } catch (e) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Write failed: $e')),
      );
    } finally {
      setState(() => _isLoading = false);
    }
  }

  @override
  void dispose() {
    for (var controller in _controllers.values) {
      controller.dispose();
    }
    super.dispose();
  }
}
```

---

## 7. Error Handling

### Error Codes

| Code | Description | User Message |
|------|-------------|--------------|
| 400 | Invalid JSON payload | "Invalid request format" |
| 400 | Multiple registers require explicit values | "Please provide value for each register" |
| 404 | Device not found | "Device not found" |
| 404 | Register not found | "Register not found" |
| 315 | Register not writable | "This register is read-only" |
| 316 | Value out of range | "Value is out of allowed range" |
| 318 | Modbus write failed | "Failed to write to device" |

### Error Handling in Flutter

```dart
void _handleWriteResponse(WriteResponse response) {
  if (response.isSuccess) {
    _showSuccess('All values written successfully');
  } else if (response.isPartial) {
    final failed = response.results.where((r) => !r.isSuccess).toList();
    _showWarning('${failed.length} register(s) failed to write');
  } else if (response.isError) {
    _showError(_getErrorMessage(response.errorCode, response.error));
  }
}

String _getErrorMessage(int? code, String? error) {
  switch (code) {
    case 400: return 'Invalid request: ${error ?? "Check your input"}';
    case 404: return 'Not found: ${error ?? "Device or register not found"}';
    case 315: return 'This register is read-only';
    case 316: return 'Value is out of allowed range';
    case 318: return 'Failed to communicate with device';
    default: return error ?? 'Unknown error occurred';
  }
}
```

---

## 8. Testing

### 8.1 Test Checklist

- [ ] Add single-register subscription via BLE
- [ ] Add multi-register subscription via BLE
- [ ] Write single value via MQTT
- [ ] Write multi-register values via MQTT
- [ ] Receive response on configured response_topic
- [ ] Handle error responses (invalid value, device offline, etc.)
- [ ] QoS levels (0, 1, 2)
- [ ] Reconnect after broker disconnect

### 8.2 Python Test Script

```python
import paho.mqtt.client as mqtt
from paho.mqtt.client import CallbackAPIVersion
import json
import time

BROKER = "192.168.1.100"
PORT = 1883

# Test single register
def test_single_register():
    client = mqtt.Client(callback_api_version=CallbackAPIVersion.VERSION2)
    client.connect(BROKER, PORT)
    client.loop_start()

    # Subscribe to response
    client.subscribe("suriota/gw001/write/temperature/response")
    client.on_message = lambda c, u, m: print(f"Response: {m.payload.decode()}")

    time.sleep(1)

    # Write value
    client.publish("suriota/gw001/write/temperature", '{"value": 25.5}', qos=1)

    time.sleep(3)
    client.disconnect()

# Test multi register
def test_multi_register():
    client = mqtt.Client(callback_api_version=CallbackAPIVersion.VERSION2)
    client.connect(BROKER, PORT)
    client.loop_start()

    client.subscribe("suriota/gw001/response/valves")
    client.on_message = lambda c, u, m: print(f"Response: {m.payload.decode()}")

    time.sleep(1)

    payload = json.dumps({"ValveA": 1, "ValveB": 0})
    client.publish("suriota/gw001/write/valves", payload, qos=1)

    time.sleep(3)
    client.disconnect()

if __name__ == "__main__":
    print("Testing single register...")
    test_single_register()

    print("\nTesting multi register...")
    test_multi_register()
```

---

**Document Version:** 2.0 | **Last Updated:** January 12, 2026

**SURIOTA R&D Team** | support@suriota.com
