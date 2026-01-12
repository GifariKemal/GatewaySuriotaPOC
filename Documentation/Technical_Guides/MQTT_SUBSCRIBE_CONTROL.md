# MQTT Subscribe Control Feature (v1.2.0)

**Version:** 1.2.0 | **Date:** January 2026 | **Status:** Implementation

---

## Executive Summary

Fitur **MQTT Subscribe Control** memungkinkan gateway untuk menerima perintah write
register dari cloud/IoT platform melalui protokol MQTT. Ini melengkapi kontrol lokal
via BLE yang sudah ada.

**Design Principle:** 1 Topic → N Registers (Topic-Centric Approach)

> **v1.2.0 Breaking Change:** Refactored dari register-centric (v1.1.0) ke
> topic-centric untuk align dengan Desktop App specification.

**Use Case:**

- Kontrol setpoint temperature dari dashboard cloud
- Multi-register control dari single topic (valves, HVAC, etc.)
- Automated control dari IoT rules engine
- SCADA integration via MQTT broker

---

## Architecture: Topic-Centric (v1.2.0)

### Why Topic-Centric?

| Aspect | v1.1.0 (Per-Register) | v1.2.0 (Topic-Centric) |
|--------|---------------------|----------------------|
| **Flexibility** | 1 topic = 1 register | 1 topic = N registers |
| **Multi-device** | Need multiple topics | Single topic controls many |
| **Config Location** | Per-register mqtt_subscribe | Server config |
| **Desktop Alignment** | No | Yes |

### Topic Structure

```
# Write Command (Cloud → Gateway)
{custom_topic}                    ← User-defined topic

# Write Response (Gateway → Cloud)
{custom_topic}/response           ← Configurable response topic

# Examples:
factory/hvac/setpoint             ← Control temperature setpoint
factory/valves/control            ← Control multiple valves
plant/production/control          ← Multi-device control
```

### Data Flow

```
┌─────────────────────────────────────────────────────────────────────┐
│  IoT Dashboard / SCADA / Node-RED                                   │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  [HVAC Control]  ──────► factory/hvac/setpoint                     │
│       └─ Slider              Payload: {"value": 25.5}              │
│                                                                     │
│  [Valve Control]  ─────► factory/valves/control                    │
│       └─ Toggle panel        Payload: {"ValveA": 1, "ValveB": 0}   │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
                            ┌───────────────┐
                            │  MQTT Broker  │
                            └───────────────┘
                                    │
                    ┌───────────────┴───────────────┐
                    │    Topic-Based Subscription   │
                    │  (from custom_subscribe_mode) │
                    └───────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────┐
│  MqttManager - Subscribe Callback                                   │
│    onMqttMessage(topic, payload)                                    │
│      ├─ Find subscription by topic match                           │
│      ├─ Parse payload JSON                                         │
│      ├─ For each register in subscription:                         │
│      │     └─ Call ModbusService.writeRegisterValue()              │
│      └─ Publish response to configured response_topic              │
└─────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
                          ┌─────────────────┐
                          │  Modbus Devices │
                          │  (FC5/FC6/FC16) │
                          └─────────────────┘
```

---

## Server Configuration (v1.2.0)

### Custom Subscribe Mode

Configuration lives in `server_config.json` under `mqtt_config`:

```json
{
  "mqtt_config": {
    "enabled": true,
    "broker_address": "192.168.1.100",
    "broker_port": 1883,
    "client_id": "mgate_001",
    "username": "user",
    "password": "pass123",

    "topic_mode": "custom_subscribe",

    "custom_subscribe_mode": {
      "enabled": true,
      "subscriptions": [
        {
          "topic": "factory/hvac/setpoint",
          "qos": 1,
          "response_topic": "factory/hvac/setpoint/response",
          "registers": [
            {"device_id": "HVAC01", "register_id": "TempSetpoint"}
          ]
        },
        {
          "topic": "factory/valves/control",
          "qos": 2,
          "response_topic": "factory/valves/status",
          "registers": [
            {"device_id": "Valve01", "register_id": "ValveA"},
            {"device_id": "Valve01", "register_id": "ValveB"}
          ]
        }
      ]
    }
  }
}
```

### Configuration Fields

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `topic_mode` | string | Yes | Must be `"custom_subscribe"` to enable |
| `custom_subscribe_mode.enabled` | boolean | Yes | Master switch |
| `custom_subscribe_mode.subscriptions` | array | Yes | Array of subscription objects |

### Subscription Object

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `topic` | string | Yes | MQTT topic to subscribe |
| `qos` | integer | No | QoS level 0, 1, or 2 (default: 1) |
| `response_topic` | string | No | Topic for response (default: {topic}/response) |
| `registers` | array | Yes | Array of register references |

### Register Reference

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `device_id` | string | Yes | ID of the Modbus device |
| `register_id` | string | Yes | ID of the register to write |

---

## Protocol Specification

### Payload Formats

**Single Register Subscription:**

```json
// Topic: factory/hvac/setpoint → 1 register
{"value": 25.5}
```

**Multi-Register Subscription:**

```json
// Topic: factory/valves/control → 2 registers (ValveA, ValveB)
{
  "ValveA": 1,
  "ValveB": 0
}
```

### Important Rules

| Scenario | Behavior |
|----------|----------|
| `{"value": X}` with 1 register | Write X to the register |
| `{"value": X}` with N registers | **ERROR** - "Multiple registers require explicit values per register_id" |
| `{"RegA": X, "RegB": Y}` with N registers | Write X to RegA, Y to RegB |

### Response Format

**Success Response:**

```json
{
  "status": "ok",
  "topic": "factory/valves/control",
  "results": [
    {
      "device_id": "Valve01",
      "register_id": "ValveA",
      "status": "ok",
      "written_value": 1,
      "raw_value": 1
    },
    {
      "device_id": "Valve01",
      "register_id": "ValveB",
      "status": "ok",
      "written_value": 0,
      "raw_value": 0
    }
  ],
  "timestamp": 1736697600000
}
```

**Partial Success Response:**

```json
{
  "status": "partial",
  "topic": "factory/valves/control",
  "results": [
    {"device_id": "Valve01", "register_id": "ValveA", "status": "ok", "written_value": 1},
    {"device_id": "Valve01", "register_id": "ValveB", "status": "error", "error": "Device offline"}
  ],
  "timestamp": 1736697600000
}
```

**Error Response:**

```json
{
  "status": "error",
  "topic": "factory/valves/control",
  "error": "Multiple registers require explicit values per register_id",
  "error_code": 400,
  "timestamp": 1736697600000
}
```

---

## Error Codes

| Code | Domain | Description |
|------|--------|-------------|
| 400 | MQTT | Invalid JSON payload |
| 400 | MQTT | Multiple registers require explicit values |
| 404 | Modbus | Device not found |
| 404 | Modbus | Register not found |
| 315 | Modbus | Register not writable |
| 316 | Modbus | Value out of range |
| 318 | Modbus | Write operation failed |

---

## BLE Configuration API

### Update Server Config with Subscriptions

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
            "topic": "factory/hvac/setpoint",
            "qos": 1,
            "registers": [
              {"device_id": "HVAC01", "register_id": "TempSetpoint"}
            ]
          }
        ]
      }
    }
  }
}
```

### Read Current Config

```json
{"op": "read", "type": "server_config"}
```

Response will include `custom_subscribe_mode` section.

---

## Testing

### Python Test Script

```python
import paho.mqtt.client as mqtt
from paho.mqtt.client import CallbackAPIVersion
import json
import time

BROKER = "192.168.1.100"
TOPIC = "factory/valves/control"
RESPONSE_TOPIC = f"{TOPIC}/response"

def on_connect(client, userdata, flags, reason_code, properties):
    print(f"Connected: {reason_code}")
    client.subscribe(RESPONSE_TOPIC)

def on_message(client, userdata, msg):
    print(f"Response: {msg.payload.decode()}")

def test_multi_register():
    client = mqtt.Client(callback_api_version=CallbackAPIVersion.VERSION2)
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(BROKER, 1883)
    client.loop_start()

    time.sleep(1)  # Wait for connection

    # Multi-register write
    payload = json.dumps({"ValveA": 1, "ValveB": 0})
    client.publish(TOPIC, payload, qos=1)
    print(f"Sent: {TOPIC} = {payload}")

    time.sleep(3)  # Wait for response
    client.loop_stop()

if __name__ == "__main__":
    test_multi_register()
```

### Test Checklist

- [ ] Single register with `{"value": X}` payload
- [ ] Multi-register with `{"RegA": X, "RegB": Y}` payload
- [ ] Error: Single value with multi-register subscription
- [ ] Response on custom response_topic
- [ ] QoS levels (0, 1, 2)
- [ ] Reconnect and resubscribe after broker disconnect
- [ ] Both RTU and TCP devices

---

## Migration from v1.1.0

If upgrading from v1.1.0:

1. **Remove** `mqtt_subscribe` from all register configurations
2. **Add** subscriptions to `server_config.mqtt_config.custom_subscribe_mode.subscriptions[]`
3. **Set** `mqtt_config.topic_mode` to `"custom_subscribe"`
4. **Update** MQTT client to use new topic format and payload structure

### Before (v1.1.0 - Register-centric):

```json
// In device register config
{
  "register_id": "TempSetpoint",
  "mqtt_subscribe": {
    "enabled": true,
    "topic_suffix": "temp_setpoint",
    "qos": 1
  }
}
```

### After (v1.2.0 - Topic-centric):

```json
// In server_config.mqtt_config
{
  "topic_mode": "custom_subscribe",
  "custom_subscribe_mode": {
    "enabled": true,
    "subscriptions": [
      {
        "topic": "factory/hvac/setpoint",
        "qos": 1,
        "registers": [
          {"device_id": "HVAC01", "register_id": "TempSetpoint"}
        ]
      }
    ]
  }
}
```

---

## Comparison: v1.1.0 vs v1.2.0

| Aspect | v1.1.0 (Register-centric) | v1.2.0 (Topic-centric) |
|--------|--------------------------|------------------------|
| Config Location | Per-register `mqtt_subscribe` | Server config `subscriptions[]` |
| Topic Format | Auto: `suriota/{gw}/write/{dev}/{suffix}` | User-defined |
| 1 Topic Controls | 1 register | N registers |
| Multi-device | Multiple topics needed | Single topic |
| Desktop App Aligned | No | Yes |
| Payload Format | `{"value": X}` or raw | `{"value": X}` or `{"RegA": X}` |

---

**Document Version:** 3.0 | **Last Updated:** January 12, 2026

**SURIOTA R&D Team** | support@suriota.com
