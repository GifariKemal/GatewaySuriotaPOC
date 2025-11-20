# MQTT Publish Modes - Documentation

**SRT-MGATE-1210 Modbus IIoT Gateway**

**Current Version:** v2.1.1
**Developer:** Kemal
**Last Updated:** November 14, 2025 (Friday) - WIB (GMT+7)

## Overview

SRT-MGATE-1210 Firmware v2.1.1 mendukung 2 mode publikasi MQTT yang dapat dikonfigurasi:
- **Default Mode**: Batch publishing - semua data dalam 1 payload
- **Customize Mode**: Multi-topic publishing - data dikelompokkan per topic dengan interval berbeda

---

## Table of Contents
1. [Configuration Structure](#configuration-structure)
2. [Default Mode](#default-mode)
3. [Customize Mode](#customize-mode)
4. [Payload Format](#payload-format)
5. [API Examples](#api-examples)
6. [Use Cases](#use-cases)

---

## Configuration Structure

### JSON Configuration Format

```json
{
  "mqtt_config": {
    "enabled": true,
    "broker_address": "demo.thingsboard.io",
    "broker_port": 1883,
    "client_id": "esp32_device",
    "username": "device_token",
    "password": "device_password",
    "keep_alive": 60,
    "clean_session": true,
    "use_tls": false,
    "publish_mode": "default",

    "default_mode": {
      "enabled": true,
      "topic_publish": "v1/devices/me/telemetry",
      "topic_subscribe": "device/control",
      "interval": 5,
      "interval_unit": "s"
    },

    "customize_mode": {
      "enabled": false,
      "custom_topics": [
        {
          "topic": "sensor/temperature",
          "registers": ["temp_room_1", "temp_room_2", "humidity_room_1"],
          "interval": 5,
          "interval_unit": "s"
        },
        {
          "topic": "sensor/pressure",
          "registers": ["pressure_inlet", "pressure_outlet"],
          "interval": 10,
          "interval_unit": "s"
        }
      ]
    }
  }
}
```

### Configuration Fields

#### Root MQTT Config
| Field            | Type    | Required | Description                                  |
| ---------------- | ------- | -------- | -------------------------------------------- |
| `enabled`        | boolean | Yes      | Enable/disable MQTT service                  |
| `broker_address` | string  | Yes      | MQTT broker hostname/IP                      |
| `broker_port`    | integer | Yes      | MQTT broker port (default: 1883)             |
| `client_id`      | string  | Yes      | Unique client identifier                     |
| `username`       | string  | No       | MQTT authentication username                 |
| `password`       | string  | No       | MQTT authentication password                 |
| `keep_alive`     | integer | No       | Keep-alive interval in seconds (default: 60) |
| `clean_session`  | boolean | No       | Clean session flag (default: true)           |
| `use_tls`        | boolean | No       | Enable TLS/SSL (default: false)              |
| `publish_mode`   | string  | Yes      | Active mode: `"default"` or `"customize"`    |

#### Default Mode Config
| Field             | Type    | Required | Description                                           |
| ----------------- | ------- | -------- | ----------------------------------------------------- |
| `enabled`         | boolean | Yes      | Enable/disable default mode                           |
| `topic_publish`   | string  | Yes      | MQTT topic for publishing                             |
| `topic_subscribe` | string  | No       | MQTT topic for subscriptions                          |
| `interval`        | integer | Yes      | Publish interval value                                |
| `interval_unit`   | string  | Yes      | Interval unit: `"ms"`, `"s"`, or `"m"` (default: "s") |

#### Customize Mode Config
| Field           | Type    | Required | Description                          |
| --------------- | ------- | -------- | ------------------------------------ |
| `enabled`       | boolean | Yes      | Enable/disable customize mode        |
| `custom_topics` | array   | Yes      | Array of custom topic configurations |

#### Custom Topic Object
| Field           | Type          | Required | Description                                           |
| --------------- | ------------- | -------- | ----------------------------------------------------- |
| `topic`         | string        | Yes      | MQTT topic name                                       |
| `registers`     | array[string] | Yes      | Array of register_id to include                       |
| `interval`      | integer       | Yes      | Publish interval value                                |
| `interval_unit` | string        | Yes      | Interval unit: `"ms"`, `"s"`, or `"m"` (default: "s") |

### Register ID Mapping

**PENTING:** Field `registers` pada custom topic menggunakan **register_id** (String), bukan address Modbus atau numeric index.

#### Apa itu register_id?

`register_id` adalah **unique identifier** untuk setiap register yang di-generate saat register dibuat via BLE CRUD API. Format umumnya: `device_id + "_" + address` atau custom string yang user-defined.

**Contoh:**
```json
// DEVICE_001 - Temperature & Humidity Sensor (4 registers)
{
  "device_id": "DEVICE_001",
  "device_name": "Temperature & Humidity Sensor",
  "registers": [
    {
      "register_id": "temp_room_1",
      "register_name": "Temperature Room 1",
      "address": 4001,
      "data_type": "FLOAT32_BE",
      "unit": "°C"
    },
    {
      "register_id": "temp_room_2",
      "register_name": "Temperature Room 2",
      "address": 4002,
      "data_type": "FLOAT32_BE",
      "unit": "°C"
    },
    {
      "register_id": "humidity_room_1",
      "register_name": "Humidity Room 1",
      "address": 4003,
      "data_type": "UINT16",
      "unit": "%"
    },
    {
      "register_id": "humidity_room_2",
      "register_name": "Humidity Room 2",
      "address": 4004,
      "data_type": "UINT16",
      "unit": "%"
    }
  ]
}

// DEVICE_003 - Power Meter (3 registers shown)
{
  "device_id": "DEVICE_003",
  "device_name": "Power Meter (3-Phase)",
  "registers": [
    {
      "register_id": "voltage_l1",
      "register_name": "Voltage L1",
      "address": 4112,
      "data_type": "FLOAT32_BE",
      "unit": "V"
    },
    {
      "register_id": "current_l1",
      "register_name": "Current L1",
      "address": 4114,
      "data_type": "FLOAT32_BE",
      "unit": "A"
    },
    {
      "register_id": "power_total",
      "register_name": "Total Power",
      "address": 4150,
      "data_type": "FLOAT32_BE",
      "unit": "W"
    }
  ]
}
```

**Penggunaan di Customize Mode:**
```json
{
  "custom_topics": [
    {
      "topic": "sensor/temperature",
      "registers": ["temp_room_1", "temp_room_2"]  // Menggunakan register_id!
    },
    {
      "topic": "power/meter",
      "registers": ["voltage_l1", "current_l1", "power_total"]
    }
  ]
}
```

Topic `sensor/temperature` akan publish data dari:
- Register `temp_room_1` (Temperature Room 1, address 4001)
- Register `temp_room_2` (Temperature Room 2, address 4002)

Topic `power/meter` akan publish data dari:
- Register `voltage_l1` (Voltage L1, address 4112)
- Register `current_l1` (Current L1, address 4114)
- Register `power_total` (Total Power, address 4150)

#### Key Benefits of Using register_id

1. **Stable Identifier:** Tidak berubah saat register di-reorder atau delete
2. **Human-Readable:** `voltage_l1` lebih jelas daripada angka `1`
3. **Unique:** Tidak ada duplikasi antar devices
4. **Backend-Friendly:** Mudah di-filter dan di-match di firmware

---

## Default Mode

### Description
Default mode mengumpulkan semua data register yang terbaca dan mengirimkannya dalam **satu payload** ke **satu topic** dengan interval tetap.

### Characteristics
- ✅ Simple configuration
- ✅ Efficient bandwidth usage (1 message instead of N messages)
- ✅ Single topic for all data
- ✅ Fixed interval for all registers
- ✅ Best for centralized data collection

### Configuration Example

```json
{
  "mqtt_config": {
    "publish_mode": "default",
    "default_mode": {
      "enabled": true,
      "topic_publish": "v1/devices/me/telemetry",
      "topic_subscribe": "device/control",
      "interval": 5,
      "interval_unit": "s"
    }
  }
}
```

### Payload Format

```json
{
  "timestamp": 1699123456789,
  "device_id": "GATEWAY_001",
  "data": [
    {
      "name": "Temperature Sensor 1",
      "value": 25.5,
      "description": "Suhu ruang server"
    },
    {
      "name": "Humidity Sensor 1",
      "value": 60.2,
      "description": "Kelembaban ruang server"
    },
    {
      "name": "Pressure Sensor 1",
      "value": 1013.25,
      "description": "Tekanan udara"
    }
  ]
}
```

### Publishing Behavior

```
Time: 0ms    → Collect data
Time: 5000ms → Publish 1 message with all data
Time: 10000ms → Publish 1 message with all data
Time: 15000ms → Publish 1 message with all data
```

### Example: 10 Registers
**Without Default Mode:**
- 10 separate MQTT messages
- More network overhead

**With Default Mode:**
- 1 MQTT message containing array of 10 data points
- Reduced bandwidth by ~90%

---

## Customize Mode

### Description
Customize mode memungkinkan pembuatan **multiple topics** dengan pemilihan register yang **fleksibel** dan **interval berbeda** per topic.

### Characteristics
- ✅ Multiple topics
- ✅ Flexible register selection per topic
- ✅ Independent interval per topic
- ✅ Registers can be included in multiple topics
- ✅ Best for categorized data or different consumers

### Configuration Example

```json
{
  "mqtt_config": {
    "publish_mode": "customize",
    "customize_mode": {
      "enabled": true,
      "custom_topics": [
        {
          "topic": "warehouse/temperature",
          "registers": ["temp_room_1", "temp_room_2", "temp_room_3"],
          "interval": 5,
          "interval_unit": "s"
        },
        {
          "topic": "warehouse/humidity",
          "registers": ["humidity_room_1", "humidity_room_2"],
          "interval": 10,
          "interval_unit": "s"
        },
        {
          "topic": "warehouse/critical",
          "registers": ["temp_room_1", "humidity_room_1"],
          "interval": 3,
          "interval_unit": "s"
        }
      ]
    }
  }
}
```

### Payload Format (Per Topic)

Topic: `warehouse/temperature`
```json
{
  "timestamp": 1699123456789,
  "data": [
    {
      "name": "Temperature Sensor 1",
      "value": 25.5,
      "description": "Suhu zona A"
    },
    {
      "name": "Temperature Sensor 2",
      "value": 26.0,
      "description": "Suhu zona B"
    },
    {
      "name": "Temperature Sensor 3",
      "value": 24.8,
      "description": "Suhu zona C"
    }
  ]
}
```

Topic: `warehouse/humidity`
```json
{
  "timestamp": 1699123456789,
  "data": [
    {
      "name": "Humidity Sensor 1",
      "value": 60.2,
      "description": "Kelembaban zona A"
    },
    {
      "name": "Humidity Sensor 2",
      "value": 58.5,
      "description": "Kelembaban zona B"
    }
  ]
}
```

### Publishing Behavior

```
Register IDs: [temp_room_1, temp_room_2, temp_room_3, humidity_room_1, humidity_room_2]

Time: 0ms     → Collect data
Time: 3000ms  → Publish to "warehouse/critical" (temp_room_1, humidity_room_1)
Time: 5000ms  → Publish to "warehouse/temperature" (temp_room_1, temp_room_2, temp_room_3)
Time: 6000ms  → Publish to "warehouse/critical" (temp_room_1, humidity_room_1)
Time: 9000ms  → Publish to "warehouse/critical" (temp_room_1, humidity_room_1)
Time: 10000ms → Publish to "warehouse/temperature" (temp_room_1, temp_room_2, temp_room_3)
Time: 10000ms → Publish to "warehouse/humidity" (humidity_room_1, humidity_room_2)
```

### Register Overlap Example

Registers can appear in multiple topics:

```json
{
  "custom_topics": [
    {
      "topic": "sensor/temperature",
      "registers": ["temp_room_1", "temp_room_2", "temp_room_3"],
      "interval": 5,
      "interval_unit": "s"
    },
    {
      "topic": "sensor/all_sensors",
      "registers": ["temp_room_1", "temp_room_2", "temp_room_3", "humidity_room_1", "pressure_inlet"],
      "interval": 10,
      "interval_unit": "s"
    },
    {
      "topic": "sensor/critical",
      "registers": ["temp_room_1", "pressure_inlet"],
      "interval": 3,
      "interval_unit": "s"
    }
  ]
}
```

**Result:**
- Register `temp_room_1`: Published to 3 topics (temperature, all_sensors, critical)
- Register `temp_room_2`, `temp_room_3`: Published to 2 topics (temperature, all_sensors)
- Register `humidity_room_1`: Published to 1 topic (all_sensors)
- Register `pressure_inlet`: Published to 2 topics (all_sensors, critical)

---

## Payload Format

### Simplified Payload (6 Fields)

Payload telah disederhanakan dari 7 fields menjadi 6 fields untuk efisiensi bandwidth dengan menambahkan field `unit` untuk measurement unit.

#### Old Payload Format (Deprecated)
```json
{
  "time": 1699123456,
  "name": "Temperature",
  "address": 40001,
  "datatype": "float",
  "value": 25.5,
  "device_id": "DEVICE_001",
  "register_id": "device_001_40001"
}
```

#### New Payload Format (Current)
```json
{
  "time": 1699123456,
  "name": "Temperature",
  "device_id": "DEVICE_001",
  "value": 25.5,
  "description": "Suhu Ruangan",
  "unit": "°C"
}
```

### Field Descriptions

| Field         | Type      | Description                                       |
| ------------- | --------- | ------------------------------------------------- |
| `time`        | integer   | Unix timestamp when data was read                 |
| `name`        | string    | Register name from configuration                  |
| `device_id`   | string    | Modbus device identifier                          |
| `value`       | float/int | Calibrated register value (after scale & offset)  |
| `description` | string    | Optional description from BLE config              |
| `unit`        | string    | Measurement unit (°C, V, A, PSI, etc.)            |

### Calibration Formula

Firmware menerapkan kalibrasi pada nilai raw Modbus **SETELAH** konversi data type menggunakan formula:

```
final_value = (raw_value × scale) + offset
```

**Default values:**
- `scale` = 1.0
- `offset` = 0.0
- `unit` = ""

**Contoh Penggunaan:**

#### Example 1: Voltage Divider (Scale Only)
```json
{
  "register_name": "Voltage Sensor",
  "scale": 0.01,
  "offset": 0.0,
  "unit": "V"
}
```
Raw Modbus: 2500 → Calibrated: (2500 × 0.01) + 0 = **25.0 V**

#### Example 2: Temperature Offset Correction
```json
{
  "register_name": "Room Temperature",
  "scale": 1.0,
  "offset": -2.5,
  "unit": "°C"
}
```
Raw Modbus: 27.5 → Calibrated: (27.5 × 1.0) - 2.5 = **25.0 °C**

#### Example 3: Fahrenheit to Celsius Conversion
```json
{
  "register_name": "Temperature (F to C)",
  "scale": 0.5556,
  "offset": -17.778,
  "unit": "°C"
}
```
Raw Modbus: 77 (°F) → Calibrated: (77 × 0.5556) - 17.778 ≈ **25.0 °C**

#### Example 4: PSI to Bar Conversion
```json
{
  "register_name": "Pressure Sensor",
  "scale": 0.06895,
  "offset": 0.0,
  "unit": "bar"
}
```
Raw Modbus: 100 (PSI) → Calibrated: (100 × 0.06895) + 0 = **6.895 bar**

### Internal Fields (Not in MQTT Payload)

These fields are used internally for deduplication and routing:

| Field         | Type   | Purpose                                                |
| ------------- | ------ | ------------------------------------------------------ |
| `register_id` | string | Unique register identifier for deduplication & routing |

### Register Configuration Changes

**PENTING:** Field `refresh_rate_ms` per-register telah dihapus. Polling interval sekarang menggunakan **device-level** `refresh_rate_ms`:

```json
{
  "op": "update",
  "type": "device",
  "device_id": "DEVICE_001",
  "config": {
    "device_name": "PM-5560",
    "protocol": "TCP",
    "refresh_rate_ms": 1000
  }
}
```

Semua register dalam device tersebut akan menggunakan interval polling yang sama (1000ms).

---

## API Examples

### 1. Enable Default Mode

**POST** `/api/server-config`

```json
{
  "mqtt_config": {
    "enabled": true,
    "broker_address": "mqtt.example.com",
    "broker_port": 1883,
    "client_id": "gateway_001",
    "publish_mode": "default",
    "default_mode": {
      "enabled": true,
      "topic_publish": "devices/gateway_001/telemetry",
      "interval": 10,
      "interval_unit": "s"
    }
  }
}
```

**Response:**
```json
{
  "status": "success",
  "message": "Server configuration updated successfully",
  "restart_scheduled": true,
  "restart_delay_seconds": 5
}
```

### 2. Enable Customize Mode

**POST** `/api/server-config`

```json
{
  "mqtt_config": {
    "enabled": true,
    "broker_address": "mqtt.example.com",
    "broker_port": 1883,
    "client_id": "gateway_001",
    "publish_mode": "customize",
    "customize_mode": {
      "enabled": true,
      "custom_topics": [
        {
          "topic": "factory/line1/temperature",
          "registers": ["temp_zone_a", "temp_zone_b", "temp_zone_c", "temp_zone_d"],
          "interval": 5,
          "interval_unit": "s"
        },
        {
          "topic": "factory/line1/pressure",
          "registers": ["pressure_inlet", "pressure_outlet"],
          "interval": 15,
          "interval_unit": "s"
        },
        {
          "topic": "factory/alerts",
          "registers": ["temp_zone_a", "pressure_inlet"],
          "interval": 2,
          "interval_unit": "s"
        }
      ]
    }
  }
}
```

### 3. Switch Between Modes

To switch from default to customize mode:

```json
{
  "mqtt_config": {
    "publish_mode": "customize",
    "default_mode": {
      "enabled": false
    },
    "customize_mode": {
      "enabled": true,
      "custom_topics": [...]
    }
  }
}
```

### 4. Disable Both Modes (MQTT Still Connected)

```json
{
  "mqtt_config": {
    "enabled": true,
    "publish_mode": "default",
    "default_mode": {
      "enabled": false
    },
    "customize_mode": {
      "enabled": false
    }
  }
}
```

Gateway will maintain MQTT connection but won't publish any data.

---

## Use Cases

### Use Case 1: IoT Platform Integration (Default Mode)

**Scenario:** Send all sensor data to ThingsBoard/AWS IoT

**Configuration:**
```json
{
  "publish_mode": "default",
  "default_mode": {
    "enabled": true,
    "topic_publish": "v1/devices/me/telemetry",
    "interval": 10,
    "interval_unit": "s"
  }
}
```

**Benefits:**
- Single message per interval
- Platform receives all data in one batch
- Easy to process on backend

---

### Use Case 2: Multi-Consumer Architecture (Customize Mode)

**Scenario:**
- Dashboard needs realtime data (1s interval)
- Database needs historical data (1min interval)
- Alert system needs critical sensors (500ms interval)

**Configuration:**
```json
{
  "publish_mode": "customize",
  "customize_mode": {
    "enabled": true,
    "custom_topics": [
      {
        "topic": "dashboard/realtime",
        "registers": ["temp_room_1", "temp_room_2", "humidity_room_1", "pressure_inlet", "voltage_l1", "current_l1"],
        "interval": 1,
        "interval_unit": "s"
      },
      {
        "topic": "database/historical",
        "registers": ["temp_room_1", "temp_room_2", "humidity_room_1", "pressure_inlet", "voltage_l1", "current_l1"],
        "interval": 1,
        "interval_unit": "m"
      },
      {
        "topic": "alerts/critical",
        "registers": ["temp_room_1", "current_l1"],
        "interval": 500,
        "interval_unit": "ms"
      }
    ]
  }
}
```

**Benefits:**
- Different consumers subscribe to different topics
- Each topic has appropriate update frequency
- Reduced network load for slow consumers

---

### Use Case 3: Categorized Data Publishing (Customize Mode)

**Scenario:** Warehouse monitoring with different sensor types

**Configuration:**
```json
{
  "publish_mode": "customize",
  "customize_mode": {
    "enabled": true,
    "custom_topics": [
      {
        "topic": "warehouse/environment/temperature",
        "registers": ["temp_zone_1", "temp_zone_2", "temp_zone_3", "temp_zone_4"],
        "interval": 5,
        "interval_unit": "s"
      },
      {
        "topic": "warehouse/environment/humidity",
        "registers": ["humidity_zone_1", "humidity_zone_2", "humidity_zone_3", "humidity_zone_4"],
        "interval": 5,
        "interval_unit": "s"
      },
      {
        "topic": "warehouse/safety/smoke",
        "registers": ["smoke_detector_1", "smoke_detector_2"],
        "interval": 1,
        "interval_unit": "s"
      },
      {
        "topic": "warehouse/safety/co2",
        "registers": ["co2_sensor_1", "co2_sensor_2"],
        "interval": 2,
        "interval_unit": "s"
      }
    ]
  }
}
```

**Benefits:**
- Logical grouping by sensor type
- Different safety sensors can have faster intervals
- Easier to implement topic-based access control

---

## Mobile App Implementation Guide

### UI Components Required

#### 1. Mode Selection Screen
- Radio button group:
  - Default Mode
  - Customize Mode
- Save button

#### 2. Default Mode Configuration Screen
```
┌─────────────────────────────────────┐
│ Default Mode Settings               │
├─────────────────────────────────────┤
│ [x] Enable Default Mode             │
│                                     │
│ Topic Publish:                      │
│ [v1/devices/me/telemetry        ]  │
│                                     │
│ Topic Subscribe (Optional):         │
│ [device/control                 ]  │
│                                     │
│ Publish Interval (ms):              │
│ [5000                           ]  │
│                                     │
│ [Save Configuration]                │
└─────────────────────────────────────┘
```

#### 3. Customize Mode Configuration Screen
```
┌─────────────────────────────────────┐
│ Customize Mode Settings             │
├─────────────────────────────────────┤
│ [x] Enable Customize Mode           │
│                                     │
│ Custom Topics:                      │
│                                     │
│ ┌─ Topic 1 ─────────────────────┐  │
│ │ Topic: [sensor/temperature  ] │  │
│ │ Registers: [1, 2, 3         ] │  │
│ │ Interval: [5000 ms          ] │  │
│ │ [Remove Topic]                 │  │
│ └────────────────────────────────┘  │
│                                     │
│ ┌─ Topic 2 ─────────────────────┐  │
│ │ Topic: [sensor/pressure     ] │  │
│ │ Registers: [4, 5            ] │  │
│ │ Interval: [10000 ms         ] │  │
│ │ [Remove Topic]                 │  │
│ └────────────────────────────────┘  │
│                                     │
│ [+ Add New Topic]                   │
│ [Save Configuration]                │
└─────────────────────────────────────┘
```

### Register Selection Widget (Hierarchical Device → Registers)

**PENTING:** Register selection menggunakan **register_id** (String unique identifier), bukan register_index atau address Modbus.

For register selection, you need a **hierarchical multi-select picker** that displays:

**Level 1: Device Selection**
- Select device first (e.g., DEVICE_001, DEVICE_003)

**Level 2: Register Selection**
- Display registers grouped by selected device
- Show **Register Name** (primary) + **register_id** (secondary)
- *Optional: Address* (untuk referensi)

```
┌─────────────────────────────────────────────┐
│ Select Device:                              │
│ ┌─ DEVICE_001 ────────────────────────┐    │
│ │ 📦 Temperature & Humidity Sensor     │    │
│ │ 4 registers available                │    │
│ └──────────────────────────────────────┘    │
│                                              │
│ ┌─ DEVICE_003 ────────────────────────┐    │
│ │ 📦 Power Meter (3-Phase)             │    │
│ │ 26 registers available               │    │
│ └──────────────────────────────────────┘    │
└─────────────────────────────────────────────┘

// User clicks DEVICE_003

┌─────────────────────────────────────────────┐
│ DEVICE_003 - Power Meter Registers:         │
│                                              │
│ [x] Voltage L1       (voltage_l1)           │
│ [x] Voltage L2       (voltage_l2)           │
│ [ ] Voltage L3       (voltage_l3)           │
│ [x] Current L1       (current_l1)           │
│ [ ] Current L2       (current_l2)           │
│ [ ] Current L3       (current_l3)           │
│ [ ] Active Power L1  (power_active_l1)      │
│ ... (20 more registers)                     │
│                                              │
│ Selected: voltage_l1, voltage_l2, current_l1│
└─────────────────────────────────────────────┘
```

**API untuk mendapatkan available registers grouped by device:**
```javascript
// GET /api/devices
// Response:
{
  "devices": [
    {
      "device_id": "DEVICE_001",
      "device_name": "Temperature & Humidity Sensor",
      "registers": [
        {
          "register_id": "temp_room_1",     // ← Gunakan ini untuk selection
          "register_name": "Temperature Room 1",
          "address": 4001
        },
        {
          "register_id": "humidity_room_1",
          "register_name": "Humidity Room 1",
          "address": 4003
        }
      ]
    },
    {
      "device_id": "DEVICE_003",
      "device_name": "Power Meter (3-Phase)",
      "registers": [
        {
          "register_id": "voltage_l1",
          "register_name": "Voltage L1",
          "address": 4112
        },
        {
          "register_id": "current_l1",
          "register_name": "Current L1",
          "address": 4114
        }
        // ... 24 more registers
      ]
    }
  ]
}
```

**UI Benefits:**
- Scalable: 5 devices × 36 registers = 180 registers, manageable
- Clear hierarchy: Device → Registers
- Reduced cognitive load: User only sees 1-36 registers at a time
- Mobile-friendly: Accordion/collapsible design

### Validation Rules

1. **Default Mode:**
   - Topic publish is required
   - Interval must be >= 1000ms

2. **Customize Mode:**
   - At least 1 custom topic required
   - Each topic must have:
     - Non-empty topic name
     - At least 1 register selected
     - Interval >= 500ms

3. **General:**
   - Only one mode can be active at a time
   - Mode can be enabled/disabled independently

### API Integration

#### Get Current Configuration
```javascript
async function getMqttConfig() {
  const response = await fetch('/api/server-config');
  const config = await response.json();
  return config.mqtt_config;
}
```

#### Update Configuration
```javascript
async function updateMqttConfig(mqttConfig) {
  const response = await fetch('/api/server-config', {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json'
    },
    body: JSON.stringify({ mqtt_config: mqttConfig })
  });

  if (response.ok) {
    alert('Configuration updated. Device will restart in 5 seconds.');
  }
}
```

#### Example: Save Default Mode
```javascript
const defaultModeConfig = {
  enabled: true,
  broker_address: "mqtt.example.com",
  broker_port: 1883,
  publish_mode: "default",
  default_mode: {
    enabled: true,
    topic_publish: "v1/devices/me/telemetry",
    topic_subscribe: "device/control",
    interval: 5000
  },
  customize_mode: {
    enabled: false
  }
};

await updateMqttConfig(defaultModeConfig);
```

#### Example: Save Customize Mode
```javascript
const customizeModeConfig = {
  enabled: true,
  broker_address: "mqtt.example.com",
  broker_port: 1883,
  publish_mode: "customize",
  default_mode: {
    enabled: false
  },
  customize_mode: {
    enabled: true,
    custom_topics: [
      {
        topic: "sensor/temperature",
        registers: ["temp_room_1", "temp_room_2", "humidity_room_1"],
        interval: 5,
        interval_unit: "s"
      },
      {
        topic: "power/meter",
        registers: ["voltage_l1", "current_l1", "power_total"],
        interval: 10,
        interval_unit: "s"
      }
    ]
  }
};

await updateMqttConfig(customizeModeConfig);
```

---

## Testing Checklist

### Default Mode Tests
- [ ] Enable default mode and verify single message published
- [ ] Verify all registers included in payload
- [ ] Verify interval timing is correct
- [ ] Test with 1 register
- [ ] Test with 50+ registers
- [ ] Verify payload format matches documentation

### Customize Mode Tests
- [ ] Enable customize mode with 1 topic
- [ ] Enable customize mode with 5+ topics
- [ ] Verify register filtering works correctly
- [ ] Verify per-topic intervals are independent
- [ ] Test register overlap (same register in multiple topics)
- [ ] Verify topics with no matching registers are skipped
- [ ] Verify payload format matches documentation

### Mode Switching Tests
- [ ] Switch from default to customize (requires restart)
- [ ] Switch from customize to default (requires restart)
- [ ] Disable both modes (MQTT stays connected)
- [ ] Enable both modes (only active mode publishes)

### Edge Cases
- [ ] Empty custom_topics array
- [ ] Topic with empty registers array (should be ignored)
- [ ] Invalid register index (should be skipped)
- [ ] Very short interval (< 500ms)
- [ ] Very long interval (> 60000ms)

---

## Troubleshooting

### No Data Published (Default Mode)
1. Check `default_mode.enabled` is `true`
2. Check `publish_mode` is set to `"default"`
3. Verify MQTT connection is established
4. Check interval value is not too long

### No Data Published (Customize Mode)
1. Check `customize_mode.enabled` is `true`
2. Check `publish_mode` is set to `"customize"`
3. Verify at least 1 topic has valid registers
4. Check `register_id` values match actual configured registers (case-sensitive)

### Data Published to Wrong Topic
1. Verify `publish_mode` setting
2. Check active mode's topic configuration

### Missing Registers in Payload
1. In customize mode, check `register_id` values in topic configuration
2. Verify `register_id` is set correctly in device config (case-sensitive)
3. Check deduplication is not removing data
4. Verify register_id exists in the configured devices

---

## Migration Guide

### From Legacy Single-Message Mode

**Old behavior:**
- Each register sent as separate MQTT message

**Migration to Default Mode:**
```json
{
  "publish_mode": "default",
  "default_mode": {
    "enabled": true,
    "topic_publish": "your/existing/topic",
    "interval": 5000
  }
}
```

**Backend changes required:**
- Update MQTT subscriber to parse array of data instead of single data point
- Loop through `payload.data[]` array

**Example subscriber update:**
```javascript
// Old code
client.on('message', (topic, message) => {
  const data = JSON.parse(message);
  processDataPoint(data); // Single data point
});

// New code
client.on('message', (topic, message) => {
  const payload = JSON.parse(message);
  payload.data.forEach(dataPoint => {
    processDataPoint(dataPoint); // Loop through array
  });
});
```

---

## Version History

### v2.1.1 (Current - November 14, 2025)
- **Developer:** Kemal
- **BREAKING CHANGE:** Customize mode now uses `register_id` (String) instead of `register_index` (int)
- Added hierarchical Device → Registers UI selection
- Improved register identification with meaningful IDs (e.g., "voltage_l1", "temp_room_1")
- Enhanced documentation with real-world examples
- Performance optimization (BLE transmission 28x faster)

### v2.0 (November 2024)
- Added Default Mode (batch publishing)
- Added Customize Mode (multi-topic publishing)
- Simplified payload from 7 fields to 6 fields (added unit field)
- Added per-topic interval support
- Added register overlap support

### v1.0 (Initial Release)
- Legacy single-message-per-register mode

---

## Support

For questions or issues:
- Firmware Developer: Kemal
- App Developer: [Your Contact]
- Documentation: This file

**Last Updated:** November 14, 2025 (Friday) - WIB (GMT+7)
**Developer:** Kemal
