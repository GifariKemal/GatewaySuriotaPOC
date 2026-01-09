# MQTT Subscribe Control - Testing Tools

**Version:** 1.1.0 | **Date:** January 2026

---

## Overview

Testing tools for the MQTT Subscribe Control feature, which allows remote write
control of Modbus registers via MQTT.

---

## Files

| File                              | Description                                |
| --------------------------------- | ------------------------------------------ |
| `mqtt_subscribe_control_gui.py`   | GUI application for interactive testing    |
| `test_ble_writable_registers.py`  | BLE test for `writable_registers` command  |
| `test_mqtt_write_simple.py`       | Simple MQTT write test (no GUI)            |

---

## Requirements

```bash
pip install paho-mqtt bleak asyncio
```

---

## 1. GUI Testing Application

Interactive GUI for testing MQTT write commands with visual feedback.

### Features

- 🔌 Connect to any MQTT broker
- 🎯 Configure gateway/device/register targets
- 📦 Multiple payload formats (raw, JSON, JSON with UUID)
- 📤 Send write commands
- 📥 View responses in real-time
- 📋 Message history log
- 🎨 Dark theme with JSON syntax highlighting

### Usage

```bash
python mqtt_subscribe_control_gui.py
```

### Screenshot

```
┌─────────────────────────────────────────────────────────────────────────┐
│ 🔌 MQTT Subscribe Control Tester                          ● Connected   │
├─────────────────────────────────────────────────────────────────────────┤
│ ┌──────────────────────┐  ┌──────────────────────────────────────────┐ │
│ │ 📡 MQTT Connection   │  │ 📤 Request                               │ │
│ │ Broker: hivemq.com   │  │ {                                        │ │
│ │ Port: 1883           │  │   "topic": "suriota/.../temp_setpoint",  │ │
│ │ Client ID: tester_1  │  │   "qos": 1,                              │ │
│ │ [Connect] [Disconnect│  │   "payload": 25.5                        │ │
│ ├──────────────────────┤  │ }                                        │ │
│ │ 🎯 Target Config     │  ├──────────────────────────────────────────┤ │
│ │ Gateway: MGate1210   │  │ 📥 Response                              │ │
│ │ Device: D7A3F2       │  │ {                                        │ │
│ │ Topic: temp_setpoint │  │   "status": "ok",                        │ │
│ │ QoS: (•)1 ( )2 ( )0  │  │   "written_value": 25.5,                 │ │
│ ├──────────────────────┤  │   "raw_value": 255                       │ │
│ │ 📦 Payload           │  │ }                                        │ │
│ │ Format: [Raw ▼]      │  │ ✅ Write successful                      │ │
│ │ Value: [25.5]        │  └──────────────────────────────────────────┘ │
│ │ [📤 Send Command]    │                                               │
│ └──────────────────────┘                                               │
├─────────────────────────────────────────────────────────────────────────┤
│ 📋 Message Log                                                          │
│ [12:34:56] Connected to MQTT broker                                     │
│ [12:34:57] Published to suriota/.../temp_setpoint: 25.5                 │
│ [12:34:58] Received response: status=ok                                 │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. BLE Writable Registers Test

Test the new `read writable_registers` BLE command.

### Usage

```bash
python test_ble_writable_registers.py
```

### Expected Response

```json
{
  "status": "ok",
  "total_writable": 5,
  "total_mqtt_enabled": 2,
  "devices": [
    {
      "device_id": "D7A3F2",
      "device_name": "Temperature Controller",
      "writable_count": 3,
      "registers": [
        {
          "register_id": "R3C8D1",
          "register_name": "temp_setpoint",
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

## 3. Simple MQTT Write Test

Command-line tool for quick MQTT write testing without GUI.

### Usage

```bash
# Basic write
python test_mqtt_write_simple.py --gateway MGate1210_A3B4C5 \
                                  --device D7A3F2 \
                                  --register temp_setpoint \
                                  --value 25.5

# With custom broker
python test_mqtt_write_simple.py --broker mqtt.example.com \
                                  --port 1883 \
                                  --gateway MGate1210_A3B4C5 \
                                  --device D7A3F2 \
                                  --register temp_setpoint \
                                  --value 100
```

---

## Test Scenarios

### Scenario 1: Basic Write

1. Open GUI application
2. Connect to broker (default: broker.hivemq.com)
3. Configure target (gateway, device, register)
4. Enter value (e.g., 25.5)
5. Click "Send Write Command"
6. Verify response shows `status: ok`

### Scenario 2: Out of Range Value

1. Configure register with min_value=0, max_value=50
2. Send value=100
3. Verify response shows `status: error`, `error_code: 316`

### Scenario 3: Non-Writable Register

1. Try to write to register with `writable: false`
2. Verify response shows `status: error`, `error_code: 315`

### Scenario 4: MQTT Subscribe Disabled

1. Try to write to register with `mqtt_subscribe.enabled: false`
2. Verify no subscription exists (topic not found)

---

## Mobile App Integration Guide

### Topic Structure

```
# Write Command (App → Gateway)
suriota/{gateway_id}/write/{device_id}/{topic_suffix}

# Write Response (Gateway → App)
suriota/{gateway_id}/write/{device_id}/{topic_suffix}/response
```

### Payload Options

```dart
// Option 1: Raw value (simplest)
mqttClient.publish(topic, "25.5");

// Option 2: JSON
mqttClient.publish(topic, '{"value": 25.5}');

// Option 3: JSON with tracking
mqttClient.publish(topic, '{"value": 25.5, "uuid": "abc-123"}');
```

### Flutter Example

```dart
class MqttWriteService {
  Future<WriteResponse> writeRegister({
    required String gatewayId,
    required String deviceId,
    required String topicSuffix,
    required double value,
  }) async {
    final topic = 'suriota/$gatewayId/write/$deviceId/$topicSuffix';
    final responseTopic = '$topic/response';

    // Subscribe to response first
    await mqttClient.subscribe(responseTopic, qos: 1);

    // Send write command
    await mqttClient.publish(topic, value.toString(), qos: 1);

    // Wait for response (with timeout)
    final response = await mqttClient
        .updates
        .where((msg) => msg.topic == responseTopic)
        .first
        .timeout(Duration(seconds: 5));

    return WriteResponse.fromJson(jsonDecode(response.payload));
  }
}
```

---

## Troubleshooting

| Issue                | Solution                                          |
| -------------------- | ------------------------------------------------- |
| No response          | Check if `subscribe_control.enabled: true`        |
| Register not found   | Verify `mqtt_subscribe.enabled: true` on register |
| Value out of range   | Check min_value/max_value on register             |
| Connection timeout   | Verify broker address and network connectivity    |
| Write failed         | Check Modbus device is online and responding      |

---

**SURIOTA R&D Team** | support@suriota.com
