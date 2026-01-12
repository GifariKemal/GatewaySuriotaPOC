# MQTT Monitor API Documentation

```
================================================================================
Document    : MQTT Monitor API Documentation
Version     : 1.3.0
Author      : Gifari K Suryo
Company     : PT Surya Inovasi Prioritas (SURIOTA)
Division    : Research & Development (R&D)
Created     : 12 Januari 2026 / January 12, 2026
================================================================================
```

## Daftar Isi | Table of Contents

- [Overview](#overview)
- [Files Modified](#files-modified)
- [Data Structures](#data-structures)
- [BLE API](#ble-api)
- [Response Format](#response-format)
- [Implementation Details](#implementation-details)
- [Usage Example](#usage-example)

---

## Overview

### Bahasa Indonesia

Fitur **MQTT Monitor** ditambahkan untuk memungkinkan aplikasi desktop/mobile memantau status koneksi MQTT, statistik publish/subscribe, dan daftar topic yang dikonfigurasi secara real-time melalui BLE.

### English

The **MQTT Monitor** feature was added to allow desktop/mobile applications to monitor MQTT connection status, publish/subscribe statistics, and configured topic lists in real-time via BLE.

### Feature Highlights

| Feature | Description |
|---------|-------------|
| Connection Status | Monitor MQTT broker connection state |
| Publish Statistics | Track successful/failed publish counts |
| Subscribe Statistics | Track received messages and write results |
| Topic Lists | View configured publish and subscribe topics |
| Uptime Tracking | Monitor connection duration and reconnect count |

---

## Files Modified

### 1. MqttManager.h

**Path:** `Main/MqttManager.h`

**Changes:**
- Added `MqttStatistics` struct for tracking MQTT operations
- Added public getter methods for statistics access
- Added new public methods: `getConnectionUptime()`, `getSubscriptionsList()`, `getPublishTopicsList()`, `getFullStatus()`

### 2. MqttManager.cpp

**Path:** `Main/MqttManager.cpp`

**Changes:**
- Initialized statistics in constructor
- Added statistics tracking in `publishPayload()` method
- Added connection tracking in `connectToMqtt()` method
- Added subscribe tracking in `handleWriteCommand()` method
- Implemented new methods for status retrieval

### 3. CRUDHandler.cpp

**Path:** `Main/CRUDHandler.cpp`

**Changes:**
- Added `get_mqtt_status` control handler

---

## Data Structures

### MqttStatistics Struct

```cpp
// Location: MqttManager.h (line ~95)

// v1.3.0: MQTT Statistics for monitoring (Desktop App MQTT Monitor feature)
struct MqttStatistics {
  uint32_t publishSuccessCount;        // Total successful publishes
  uint32_t publishFailCount;           // Total failed publishes
  unsigned long lastPublishTimestamp;  // Last successful publish time (millis)

  uint32_t subscribeReceivedCount;     // Total subscribe messages received
  uint32_t subscribeWriteSuccessCount; // Successful register writes from subscribe
  uint32_t subscribeWriteFailCount;    // Failed register writes from subscribe
  unsigned long lastSubscribeTimestamp;// Last subscribe message time (millis)

  unsigned long connectionStartTime;   // Connection start time (millis)
  uint32_t reconnectCount;             // Number of reconnections
};
```

### Statistics Initialization

```cpp
// Location: MqttManager.cpp - Constructor (line ~71-80)

// v1.3.0: Initialize MQTT statistics
stats.publishSuccessCount = 0;
stats.publishFailCount = 0;
stats.lastPublishTimestamp = 0;
stats.subscribeReceivedCount = 0;
stats.subscribeWriteSuccessCount = 0;
stats.subscribeWriteFailCount = 0;
stats.lastSubscribeTimestamp = 0;
stats.connectionStartTime = 0;
stats.reconnectCount = 0;
```

---

## BLE API

### Command: get_mqtt_status

**Operation:** `control`
**Type:** `get_mqtt_status`

#### Request Format

```json
{
  "op": "control",
  "type": "get_mqtt_status"
}
```

#### Response Format

```json
{
  "status": "ok",
  "command": "get_mqtt_status",
  "data": {
    "running": true,
    "mqtt_connected": true,
    "broker_address": "broker.hivemq.com",
    "broker_port": 1883,
    "client_id": "gwsrt_ABC123",
    "queue_size": 0,
    "publish_mode": "default",
    "statistics": {
      "publish_success_count": 150,
      "publish_fail_count": 2,
      "last_publish_timestamp": 5520000,
      "subscribe_received_count": 10,
      "subscribe_write_success_count": 8,
      "subscribe_write_fail_count": 2,
      "last_subscribe_timestamp": 5515000,
      "connection_uptime_ms": 3600000,
      "reconnect_count": 1
    },
    "publish_topics": [
      {
        "topic": "v1/devices/me/telemetry/gwsrt",
        "interval": 10,
        "interval_unit": "s"
      }
    ],
    "subscribe_control": {
      "custom_subscribe_enabled": true,
      "base_topic": "suriota/gwsrt/write"
    },
    "subscriptions": [
      {
        "topic": "suriota/gwsrt/write/device1/temp_setpoint",
        "qos": 1,
        "registers": ["temp_setpoint"]
      }
    ]
  }
}
```

---

## Response Format

### Response Fields Description

| Field | Type | Description |
|-------|------|-------------|
| `running` | boolean | Whether MQTT service is running |
| `mqtt_connected` | boolean | Current connection status to broker |
| `broker_address` | string | MQTT broker hostname/IP |
| `broker_port` | integer | MQTT broker port |
| `client_id` | string | MQTT client identifier |
| `queue_size` | integer | Number of messages in publish queue |
| `publish_mode` | string | Current publish mode ("default" or "custom") |

### Statistics Fields

| Field | Type | Description |
|-------|------|-------------|
| `publish_success_count` | integer | Total successful publish operations |
| `publish_fail_count` | integer | Total failed publish operations |
| `last_publish_timestamp` | integer | Timestamp of last publish (millis since boot) |
| `subscribe_received_count` | integer | Total subscribe messages received |
| `subscribe_write_success_count` | integer | Successful register writes from subscribe |
| `subscribe_write_fail_count` | integer | Failed register writes from subscribe |
| `last_subscribe_timestamp` | integer | Timestamp of last subscribe message |
| `connection_uptime_ms` | integer | Connection duration in milliseconds |
| `reconnect_count` | integer | Number of reconnections since boot |

### Publish Topics Array

| Field | Type | Description |
|-------|------|-------------|
| `topic` | string | Full MQTT topic path |
| `interval` | integer | Publish interval value |
| `interval_unit` | string | Interval unit ("s" for seconds, "m" for minutes) |

### Subscribe Control Object

| Field | Type | Description |
|-------|------|-------------|
| `custom_subscribe_enabled` | boolean | Whether custom subscribe is enabled |
| `base_topic` | string | Base topic for subscribe (if custom enabled) |

### Subscriptions Array

| Field | Type | Description |
|-------|------|-------------|
| `topic` | string | Full subscription topic |
| `qos` | integer | Quality of Service level (0, 1, or 2) |
| `registers` | array | List of register names linked to this topic |

---

## Implementation Details

### 1. Statistics Tracking in publishPayload()

```cpp
// Location: MqttManager.cpp - publishPayload() (line ~794-800)

// v1.3.0: Update MQTT statistics for Desktop App MQTT Monitor
if (published) {
  stats.publishSuccessCount++;
  stats.lastPublishTimestamp = millis();
} else {
  stats.publishFailCount++;
}
```

### 2. Connection Tracking in connectToMqtt()

```cpp
// Location: MqttManager.cpp - connectToMqtt() (line ~413-418)

// v1.3.0: Track connection statistics for Desktop App MQTT Monitor
if (stats.connectionStartTime == 0) {
  stats.connectionStartTime = millis();  // First connection
} else {
  stats.reconnectCount++;  // Reconnection
}
```

### 3. Subscribe Statistics in handleWriteCommand()

```cpp
// Location: MqttManager.cpp - handleWriteCommand() (line ~1935-1939)

// v1.3.0: Update MQTT subscribe statistics for Desktop App MQTT Monitor
stats.subscribeReceivedCount++;
stats.lastSubscribeTimestamp = millis();
stats.subscribeWriteSuccessCount += successCount;
stats.subscribeWriteFailCount += failCount;
```

### 4. Connection Uptime Calculation

```cpp
// Location: MqttManager.cpp (line ~1960-1965)

unsigned long MqttManager::getConnectionUptime() const {
  if (stats.connectionStartTime == 0 || !mqttClient.connected()) {
    return 0;
  }
  return millis() - stats.connectionStartTime;
}
```

### 5. Get Full Status Method

```cpp
// Location: MqttManager.cpp (line ~2005-2050)

void MqttManager::getFullStatus(JsonObject& status) {
  // Basic status
  status["running"] = running;
  status["mqtt_connected"] = mqttClient.connected();
  status["broker_address"] = config.host;
  status["broker_port"] = config.port;
  status["client_id"] = config.clientId;
  status["queue_size"] = publishQueue.size();
  status["publish_mode"] = config.customPublishEnabled ? "custom" : "default";

  // Statistics
  JsonObject stats_obj = status["statistics"].to<JsonObject>();
  stats_obj["publish_success_count"] = stats.publishSuccessCount;
  stats_obj["publish_fail_count"] = stats.publishFailCount;
  stats_obj["last_publish_timestamp"] = stats.lastPublishTimestamp;
  stats_obj["subscribe_received_count"] = stats.subscribeReceivedCount;
  stats_obj["subscribe_write_success_count"] = stats.subscribeWriteSuccessCount;
  stats_obj["subscribe_write_fail_count"] = stats.subscribeWriteFailCount;
  stats_obj["last_subscribe_timestamp"] = stats.lastSubscribeTimestamp;
  stats_obj["connection_uptime_ms"] = getConnectionUptime();
  stats_obj["reconnect_count"] = stats.reconnectCount;

  // Publish topics
  JsonArray topics = status["publish_topics"].to<JsonArray>();
  getPublishTopicsList(topics);

  // Subscribe control
  JsonObject subCtrl = status["subscribe_control"].to<JsonObject>();
  subCtrl["custom_subscribe_enabled"] = config.customSubscribeEnabled;
  if (config.customSubscribeEnabled) {
    subCtrl["base_topic"] = config.subscribeTopic;
  }

  // Subscriptions list
  JsonArray subs = status["subscriptions"].to<JsonArray>();
  getSubscriptionsList(subs);
}
```

### 6. CRUDHandler Control Handler

```cpp
// Location: CRUDHandler.cpp (line ~1523-1544)

// === v1.3.0: MQTT STATUS API (Desktop App MQTT Monitor) ===
controlHandlers["get_mqtt_status"] = [this](BLEManager* manager,
                                           const JsonDocument& command) {
  MqttManager* mqttMgr = MqttManager::getInstance();
  if (!mqttMgr) {
    manager->sendError("MQTT manager not initialized", "control");
    return;
  }

  auto response = make_psram_unique<JsonDocument>();
  (*response)["status"] = "ok";
  (*response)["command"] = "get_mqtt_status";

  JsonObject data = (*response)["data"].to<JsonObject>();
  mqttMgr->getFullStatus(data);

  manager->sendResponse(*response);
  LOG_CRUD_INFO("[CRUD] MQTT status sent");
};
```

---

## Usage Example

### Desktop App (Flutter/Dart)

```dart
// Send command to get MQTT status
final response = await bleController.sendCommand({
  'op': 'control',
  'type': 'get_mqtt_status',
});

if (response.success && response.data != null) {
  final status = response.data as Map<String, dynamic>;

  // Connection status
  final isConnected = status['mqtt_connected'] == true;
  final broker = status['broker_address'];
  final port = status['broker_port'];

  // Statistics
  final stats = status['statistics'] as Map<String, dynamic>;
  final publishSuccess = stats['publish_success_count'];
  final publishFail = stats['publish_fail_count'];
  final uptimeMs = stats['connection_uptime_ms'];

  // Topics
  final publishTopics = status['publish_topics'] as List;
  final subscriptions = status['subscriptions'] as List;

  print('MQTT Connected: $isConnected');
  print('Broker: $broker:$port');
  print('Publish Success: $publishSuccess, Fail: $publishFail');
  print('Uptime: ${uptimeMs / 1000 / 60} minutes');
}
```

### Serial Monitor Output Example

When `get_mqtt_status` command is received:

```
[2026-01-12 17:20:00][INFO][CRUD] [CRUD] MQTT status sent
```

---

## Error Handling

### Possible Error Responses

| Error | Cause | Solution |
|-------|-------|----------|
| "MQTT manager not initialized" | MqttManager singleton not created | Check MQTT configuration and restart gateway |
| "Unknown operation or type" | Invalid command format | Use correct format: `{"op": "control", "type": "get_mqtt_status"}` |

---

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.3.0 | 2026-01-12 | Initial implementation of MQTT Monitor API |

---

## Related Files

### Firmware

- `Main/MqttManager.h` - Header file with struct and method declarations
- `Main/MqttManager.cpp` - Implementation of MQTT management and statistics
- `Main/CRUDHandler.cpp` - BLE command handler implementation

### Desktop App

- `lib/presentation/pages/mqtt_monitor/mqtt_monitor_screen.dart` - MQTT Monitor UI
- `lib/core/controllers/navigation_controller.dart` - Navigation indices
- `lib/presentation/widgets/desktop/sidebar_navigation.dart` - Sidebar menu

---

```
================================================================================
Copyright (c) 2026 PT Surya Inovasi Prioritas (SURIOTA)
All Rights Reserved
================================================================================
```
