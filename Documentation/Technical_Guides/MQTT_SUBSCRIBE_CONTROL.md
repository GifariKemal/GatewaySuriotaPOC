# MQTT Subscribe Control Feature

**Version:** 1.1.0 | **Date:** January 2026 | **Status:** Implementation

---

## Executive Summary

Fitur **MQTT Subscribe Control** memungkinkan gateway untuk menerima perintah write
register dari cloud/IoT platform melalui protokol MQTT. Ini melengkapi kontrol lokal
via BLE yang sudah ada (v1.0.8), memberikan kemampuan kontrol remote secara online.

**Design Principle:** 1 Topic = 1 Register (Per-Register Topic Approach)

**Use Case:**

- Kontrol setpoint temperature dari dashboard cloud
- Toggle relay/coil dari aplikasi mobile via internet
- Automated control dari IoT rules engine
- SCADA integration via MQTT broker

---

## Architecture: Per-Register Topic

### Why Per-Register Topic?

| Aspect | Single Control Topic | Per-Register Topic |
|--------|---------------------|-------------------|
| **Security** | ACL sulit (perlu parse payload) | ACL mudah (per-topic) |
| **Clarity** | Perlu baca payload | Langsung jelas dari topic |
| **Dashboard** | Perlu routing logic | Widget langsung bind |
| **Simplicity** | Complex payload | Simple value payload |

### Topic Structure

```
# Write Command (Cloud → Gateway)
suriota/{gateway_id}/write/{device_id}/{register_id}

# Write Response (Gateway → Cloud)
suriota/{gateway_id}/write/{device_id}/{register_id}/response

# Examples:
suriota/MGate1210_A3B4C5/write/D7A3F2/temp_setpoint          ← Subscribe
suriota/MGate1210_A3B4C5/write/D7A3F2/temp_setpoint/response → Publish
```

### Data Flow

```
┌─────────────────────────────────────────────────────────────────────┐
│  IoT Dashboard / SCADA / Node-RED                                   │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  [Temp Setpoint: 25.5°C]  ──► .../write/D7A3F2/temp_setpoint       │
│       └─ Slider widget            Payload: 25.5                     │
│                                                                     │
│  [Fan Speed: 75%]  ───────► .../write/D7A3F2/fan_speed             │
│       └─ Slider widget            Payload: 75                       │
│                                                                     │
│  [Relay 1: ON]  ──────────► .../write/A1B2C3/relay_1               │
│       └─ Toggle switch            Payload: 1                        │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
                            ┌───────────────┐
                            │  MQTT Broker  │
                            └───────────────┘
                                    │
                    ┌───────────────┴───────────────┐
                    │     Per-Topic Subscription    │
                    │  (only enabled registers)     │
                    └───────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────┐
│  MqttManager - Subscribe Callback                                   │
│    onMqttMessage(topic, payload)                                    │
│      ├─ Parse topic → extract device_id, register_id                │
│      ├─ Validate register is mqtt_subscribe enabled                 │
│      ├─ Parse payload → get value                                   │
│      ├─ Call ModbusService.writeRegisterValue()                     │
│      └─ Publish response to .../response topic                      │
└─────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
                          ┌─────────────────┐
                          │  Modbus Device  │
                          │  (FC5/FC6/FC16) │
                          └─────────────────┘
```

---

## Register Configuration

### New `mqtt_subscribe` Field

Setiap register yang ingin dikontrol via MQTT harus memiliki:

1. `writable: true` - Register harus writable
2. `mqtt_subscribe.enabled: true` - Explicitly enable MQTT control

```json
{
  "register_id": "R3C8D1",
  "register_name": "temp_setpoint",
  "address": 100,
  "function_code": 3,
  "data_type": "INT16",
  "scale": 0.1,
  "offset": 0,
  "unit": "°C",

  "writable": true,
  "min_value": 0,
  "max_value": 50,

  "mqtt_subscribe": {
    "enabled": true,
    "topic_suffix": "temp_setpoint",
    "qos": 1
  }
}
```

### Field Definitions

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `mqtt_subscribe` | object | No | MQTT subscribe configuration |
| `mqtt_subscribe.enabled` | boolean | Yes | Enable/disable MQTT control |
| `mqtt_subscribe.topic_suffix` | string | No | Custom topic suffix (default: register_id) |
| `mqtt_subscribe.qos` | integer | No | QoS level 0, 1, or 2 (default: 1) |

### Safety Rules

1. **Must be writable** - `writable: true` is prerequisite
2. **Explicit opt-in** - `mqtt_subscribe.enabled: true` required
3. **Validation applied** - `min_value`/`max_value` still enforced
4. **Calibration applied** - `scale`/`offset` reverse calibration applied

---

## Protocol Specification

### Write Command

**Topic:**
```
suriota/{gateway_id}/write/{device_id}/{topic_suffix}
```

**Payload Options:**

```
# Option 1: Raw value (simplest)
25.5

# Option 2: JSON with value
{"value": 25.5}

# Option 3: JSON with metadata (for tracking)
{"value": 25.5, "uuid": "550e8400-e29b-41d4-a716-446655440000"}
```

**Examples:**
```
Topic: suriota/MGate1210_A3B4C5/write/D7A3F2/temp_setpoint
Payload: 25.5

Topic: suriota/MGate1210_A3B4C5/write/D7A3F2/relay_1
Payload: 1

Topic: suriota/MGate1210_A3B4C5/write/A1B2C3/fan_speed
Payload: {"value": 75}
```

### Write Response

**Topic:**
```
suriota/{gateway_id}/write/{device_id}/{topic_suffix}/response
```

**Success Response:**
```json
{
  "status": "ok",
  "device_id": "D7A3F2",
  "register_id": "R3C8D1",
  "register_name": "temp_setpoint",
  "written_value": 25.5,
  "raw_value": 255,
  "timestamp": 1704067200000
}
```

**Error Response:**
```json
{
  "status": "error",
  "device_id": "D7A3F2",
  "register_id": "R3C8D1",
  "error_code": 316,
  "error": "Value out of range",
  "min_value": 0,
  "max_value": 50,
  "received_value": 100,
  "timestamp": 1704067200000
}
```

---

## Server Configuration

### Global MQTT Subscribe Settings

```json
{
  "mqtt_config": {
    "enabled": true,
    "broker_address": "broker.hivemq.com",
    "broker_port": 1883,
    "client_id": "MGate1210_A3B4C5",
    "username": "",
    "password": "",

    "publish_mode": "default",
    "default_mode": {
      "enabled": true,
      "topic_publish": "suriota/MGate1210_A3B4C5/telemetry",
      "interval": 5,
      "interval_unit": "s"
    },

    "subscribe_control": {
      "enabled": true,
      "topic_prefix": "suriota/MGate1210_A3B4C5/write",
      "response_enabled": true,
      "default_qos": 1
    }
  }
}
```

### Configuration Fields

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `subscribe_control.enabled` | boolean | false | Master switch for MQTT control |
| `subscribe_control.topic_prefix` | string | auto | Base topic for write commands |
| `subscribe_control.response_enabled` | boolean | true | Publish write responses |
| `subscribe_control.default_qos` | integer | 1 | Default QoS for subscriptions |

---

## Implementation Details

### MqttManager Changes

```cpp
// MqttManager.h - New methods

class MqttManager {
private:
  // Subscribe control
  bool subscribeControlEnabled;
  String subscribeTopicPrefix;
  bool responseEnabled;

  // Subscription tracking
  std::vector<SubscribedRegister> subscribedRegisters;

  // Callback
  static void onMqttMessage(char* topic, byte* payload, unsigned int length);

public:
  // Subscribe management
  void initializeSubscriptions();
  void subscribeToRegister(const String& deviceId, const String& registerId,
                           const String& topicSuffix, uint8_t qos);
  void unsubscribeFromRegister(const String& deviceId, const String& registerId);
  void resubscribeAll();

  // Message handling
  void handleWriteCommand(const String& deviceId, const String& topicSuffix,
                          const String& payload);
  void publishWriteResponse(const String& deviceId, const String& topicSuffix,
                            JsonDocument& response);
};
```

### Subscription Initialization Flow

```cpp
void MqttManager::initializeSubscriptions() {
  if (!subscribeControlEnabled) return;

  ConfigManager* config = ConfigManager::getInstance();
  JsonDocument devices;
  config->readAllDevices(devices);

  for (JsonObject device : devices["devices"].as<JsonArray>()) {
    String deviceId = device["device_id"];

    for (JsonObject reg : device["registers"].as<JsonArray>()) {
      // Check if register is writable AND mqtt_subscribe enabled
      if (reg["writable"] == true &&
          reg["mqtt_subscribe"]["enabled"] == true) {

        String registerId = reg["register_id"];
        String topicSuffix = reg["mqtt_subscribe"]["topic_suffix"] | registerId;
        uint8_t qos = reg["mqtt_subscribe"]["qos"] | 1;

        subscribeToRegister(deviceId, registerId, topicSuffix, qos);
      }
    }
  }
}
```

### Message Callback

```cpp
void MqttManager::onMqttMessage(char* topic, byte* payload, unsigned int length) {
  // Parse topic: suriota/{gateway}/write/{device_id}/{topic_suffix}
  String topicStr = String(topic);

  // Extract device_id and topic_suffix from topic
  // ... parsing logic ...

  // Convert payload to string
  String payloadStr;
  for (unsigned int i = 0; i < length; i++) {
    payloadStr += (char)payload[i];
  }

  // Handle write command
  getInstance()->handleWriteCommand(deviceId, topicSuffix, payloadStr);
}
```

### Write Command Handler

```cpp
void MqttManager::handleWriteCommand(const String& deviceId,
                                      const String& topicSuffix,
                                      const String& payload) {
  // 1. Find register by device_id + topic_suffix
  ConfigManager* config = ConfigManager::getInstance();
  JsonDocument regDoc;
  String registerId;

  if (!findRegisterByTopicSuffix(deviceId, topicSuffix, regDoc, registerId)) {
    publishErrorResponse(deviceId, topicSuffix, 404, "Register not found");
    return;
  }

  // 2. Parse value from payload
  float value;
  if (!parsePayloadValue(payload, value)) {
    publishErrorResponse(deviceId, topicSuffix, 400, "Invalid payload format");
    return;
  }

  // 3. Reuse existing write logic from v1.0.8
  JsonDocument device;
  config->readDevice(deviceId, device);
  String protocol = device["protocol"] | "RTU";

  bool success;
  JsonDocument response;

  if (protocol == "TCP") {
    success = ModbusTcpService::getInstance()->writeRegisterValue(
      deviceId, registerId, value, response);
  } else {
    success = ModbusRtuService::getInstance()->writeRegisterValue(
      deviceId, registerId, value, response);
  }

  // 4. Publish response
  if (responseEnabled) {
    publishWriteResponse(deviceId, topicSuffix, response);
  }
}
```

---

## Error Codes

| Code | Domain | Description |
|------|--------|-------------|
| 400 | MQTT | Invalid payload format |
| 404 | MQTT | Register not found for topic |
| 405 | MQTT | MQTT subscribe not enabled for register |
| 315 | Modbus | Register not writable |
| 316 | Modbus | Value out of range |
| 318 | Modbus | Write operation failed |

---

## BLE API for MQTT Subscribe Configuration

### Enable MQTT Subscribe on Register

```json
{
  "op": "update",
  "type": "register",
  "device_id": "D7A3F2",
  "register_id": "R3C8D1",
  "config": {
    "writable": true,
    "min_value": 0,
    "max_value": 50,
    "mqtt_subscribe": {
      "enabled": true,
      "topic_suffix": "temp_setpoint",
      "qos": 1
    }
  }
}
```

### Disable MQTT Subscribe

```json
{
  "op": "update",
  "type": "register",
  "device_id": "D7A3F2",
  "register_id": "R3C8D1",
  "config": {
    "mqtt_subscribe": {
      "enabled": false
    }
  }
}
```

### Read MQTT Subscribe Status

Response dari `read register` akan include:

```json
{
  "status": "ok",
  "register": {
    "register_id": "R3C8D1",
    "register_name": "temp_setpoint",
    "writable": true,
    "mqtt_subscribe": {
      "enabled": true,
      "topic_suffix": "temp_setpoint",
      "topic_full": "suriota/MGate1210_A3B4C5/write/D7A3F2/temp_setpoint",
      "qos": 1
    }
  }
}
```

### List Writable Registers (NEW v1.1.0)

Command khusus untuk mendapatkan semua register yang bisa di-write, grouped by device.
Digunakan mobile app untuk menampilkan UI konfigurasi MQTT Subscribe.

**Request:**

```json
{
  "op": "read",
  "type": "writable_registers"
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
      "writable_count": 3,
      "registers": [
        {
          "register_id": "R3C8D1",
          "register_name": "temp_setpoint",
          "address": 100,
          "data_type": "INT16",
          "min_value": 0,
          "max_value": 50,
          "mqtt_subscribe": {
            "enabled": true,
            "topic_suffix": "temp_setpoint",
            "qos": 1
          }
        },
        {
          "register_id": "R4D9E2",
          "register_name": "fan_speed",
          "address": 101,
          "data_type": "UINT16",
          "min_value": 0,
          "max_value": 100,
          "mqtt_subscribe": {
            "enabled": true,
            "topic_suffix": "fan_speed",
            "qos": 1
          }
        },
        {
          "register_id": "R5E0F3",
          "register_name": "mode_select",
          "address": 102,
          "data_type": "INT16",
          "mqtt_subscribe": {
            "enabled": false
          }
        }
      ]
    },
    {
      "device_id": "A1B2C3",
      "device_name": "Relay Controller",
      "writable_count": 2,
      "registers": [
        {
          "register_id": "R6F1G4",
          "register_name": "relay_1",
          "address": 0,
          "data_type": "COIL",
          "mqtt_subscribe": {
            "enabled": false
          }
        }
      ]
    }
  ]
}
```

**Response Fields:**

| Field | Type | Description |
|-------|------|-------------|
| `total_writable` | integer | Total jumlah register yang writable |
| `total_mqtt_enabled` | integer | Total register dengan mqtt_subscribe.enabled=true |
| `devices` | array | List device yang memiliki writable registers |
| `devices[].writable_count` | integer | Jumlah writable registers di device ini |

---

## Testing

### Python Test Script

```python
import paho.mqtt.client as mqtt
import json
import time

BROKER = "broker.hivemq.com"
GATEWAY_ID = "MGate1210_A3B4C5"
DEVICE_ID = "D7A3F2"
REGISTER_SUFFIX = "temp_setpoint"

def on_message(client, userdata, msg):
    print(f"Response: {msg.topic}")
    print(f"Payload: {msg.payload.decode()}")

def test_write_register():
    client = mqtt.Client()
    client.on_message = on_message
    client.connect(BROKER, 1883)

    # Subscribe to response topic
    response_topic = f"suriota/{GATEWAY_ID}/write/{DEVICE_ID}/{REGISTER_SUFFIX}/response"
    client.subscribe(response_topic)
    client.loop_start()

    # Send write command
    write_topic = f"suriota/{GATEWAY_ID}/write/{DEVICE_ID}/{REGISTER_SUFFIX}"
    client.publish(write_topic, "25.5")

    print(f"Sent: {write_topic} = 25.5")

    time.sleep(5)  # Wait for response
    client.loop_stop()

if __name__ == "__main__":
    test_write_register()
```

### Test Checklist

- [ ] Write single register via MQTT
- [ ] Verify response on response topic
- [ ] Write with invalid value (out of range)
- [ ] Write to non-enabled register (should fail)
- [ ] Write to non-writable register (should fail)
- [ ] Concurrent BLE + MQTT writes
- [ ] Reconnect and resubscribe after broker disconnect

---

## Comparison: BLE vs MQTT Control

| Aspect | BLE Control (v1.0.8) | MQTT Control (v1.1.0) |
|--------|---------------------|----------------------|
| Range | Local (~10m) | Global (Internet) |
| Latency | Low (~50ms) | Medium (~200-500ms) |
| Security | BLE pairing | Broker auth + TLS |
| Topic | N/A | Per-register |
| Use Case | Mobile app config | Cloud/SCADA control |
| Concurrent | 1 client | Multiple clients |

---

## Mobile App Integration

Untuk mobile app yang ingin menggunakan MQTT control:

```dart
class MqttWriteService {
  final String brokerHost;
  final String gatewayId;

  Future<void> writeRegister({
    required String deviceId,
    required String topicSuffix,
    required dynamic value,
  }) async {
    final topic = 'suriota/$gatewayId/write/$deviceId/$topicSuffix';

    // Simple value publish
    await mqttClient.publish(topic, value.toString(), qos: 1);
  }

  Stream<WriteResponse> subscribeToResponses(String deviceId, String topicSuffix) {
    final responseTopic = 'suriota/$gatewayId/write/$deviceId/$topicSuffix/response';
    return mqttClient.subscribe(responseTopic).map((msg) =>
      WriteResponse.fromJson(jsonDecode(msg.payload)));
  }
}
```

---

**Document Version:** 2.0 | **Last Updated:** January 2026

**SURIOTA R&D Team** | support@suriota.com
