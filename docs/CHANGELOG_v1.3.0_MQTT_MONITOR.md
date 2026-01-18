# Changelog v1.3.0 - MQTT Monitor Feature

```
================================================================================
Document    : Changelog v1.3.0 - MQTT Monitor Feature
Version     : 1.3.0
Author      : Gifari K Suryo
Company     : PT Surya Inovasi Prioritas (SURIOTA)
Division    : Research & Development (R&D)
Created     : 12 Januari 2026 / January 12, 2026
================================================================================
```

## Summary

Penambahan fitur **MQTT Monitor** yang memungkinkan pemantauan status MQTT secara real-time melalui BLE dari aplikasi desktop.

Addition of **MQTT Monitor** feature that enables real-time MQTT status monitoring via BLE from desktop application.

---

## Bug Fixes (v1.3.0 Patch)

### Fix 1: Timestamp "Last publish" menampilkan nilai salah

**Problem:** Desktop app menghitung `DateTime.now() - millis()` yang menghasilkan nilai huge karena `millis()` adalah waktu sejak boot (e.g., 60000ms), bukan Unix timestamp.

**Solution:**
- Firmware: Tambah field `gateway_uptime_ms` yang berisi `millis()` saat ini
- Desktop: Hitung `gateway_uptime_ms - last_publish_timestamp` untuk mendapat "X seconds ago"

### Fix 2: Interval menampilkan "10000s" instead of "10s"

**Problem:** Firmware menyimpan interval dalam milliseconds internal (10000ms untuk 10s), tapi mengembalikan nilai ms dengan unit "s" → tampil "10000s".

**Solution:**
- Tambah fungsi `convertFromMilliseconds()` untuk convert balik ke unit asli
- Update `getPublishTopicsList()` untuk menggunakan fungsi ini

---

## Code Changes Detail

### File 1: MqttManager.h

**Location:** `Main/MqttManager.h`

#### Addition 1: MqttStatistics Struct (after line ~93)

```cpp
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

#### Addition 2: Stats Member Variable (in private section)

```cpp
MqttStatistics stats;
```

#### Addition 3: Public Getter Methods (in public section)

```cpp
// v1.3.0: Statistics getters for Desktop App MQTT Monitor
uint32_t getPublishSuccessCount() const { return stats.publishSuccessCount; }
uint32_t getPublishFailCount() const { return stats.publishFailCount; }
unsigned long getLastPublishTimestamp() const { return stats.lastPublishTimestamp; }
uint32_t getSubscribeReceivedCount() const { return stats.subscribeReceivedCount; }
uint32_t getSubscribeWriteSuccessCount() const { return stats.subscribeWriteSuccessCount; }
uint32_t getSubscribeWriteFailCount() const { return stats.subscribeWriteFailCount; }
unsigned long getLastSubscribeTimestamp() const { return stats.lastSubscribeTimestamp; }
unsigned long getConnectionUptime() const;
uint32_t getReconnectCount() const { return stats.reconnectCount; }

// v1.3.0: Full status methods for Desktop App MQTT Monitor
void getSubscriptionsList(JsonArray& subs) const;
void getPublishTopicsList(JsonArray& topics) const;
void getFullStatus(JsonObject& status);
```

---

### File 2: MqttManager.cpp

**Location:** `Main/MqttManager.cpp`

#### Addition 1: Stats Initialization in Constructor (line ~71-80)

```cpp
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

#### Addition 2: Connection Tracking in connectToMqtt() (line ~413-418)

```cpp
// v1.3.0: Track connection statistics for Desktop App MQTT Monitor
if (stats.connectionStartTime == 0) {
  stats.connectionStartTime = millis();  // First connection
} else {
  stats.reconnectCount++;  // Reconnection
}
```

#### Addition 3: Publish Statistics in publishPayload() (line ~794-800)

```cpp
// v1.3.0: Update MQTT statistics for Desktop App MQTT Monitor
if (published) {
  stats.publishSuccessCount++;
  stats.lastPublishTimestamp = millis();
} else {
  stats.publishFailCount++;
}
```

#### Addition 4: Subscribe Statistics in handleWriteCommand() (line ~1935-1939)

```cpp
// v1.3.0: Update MQTT subscribe statistics for Desktop App MQTT Monitor
stats.subscribeReceivedCount++;
stats.lastSubscribeTimestamp = millis();
stats.subscribeWriteSuccessCount += successCount;
stats.subscribeWriteFailCount += failCount;
```

#### Addition 5: New Methods at End of File (line ~1960+)

```cpp
// ============================================================================
// v1.3.0: MQTT Monitor Methods (Desktop App MQTT Monitor feature)
// ============================================================================

unsigned long MqttManager::getConnectionUptime() const {
  if (stats.connectionStartTime == 0 || !mqttClient.connected()) {
    return 0;
  }
  return millis() - stats.connectionStartTime;
}

void MqttManager::getSubscriptionsList(JsonArray& subs) const {
  if (!config.customSubscribeEnabled) return;

  // Iterate through devices and registers to find mqtt_subscribe enabled
  DeviceConfigManager* deviceMgr = DeviceConfigManager::getInstance();
  if (!deviceMgr) return;

  const auto& devices = deviceMgr->getDevices();
  for (const auto& device : devices) {
    for (const auto& reg : device.registers) {
      if (reg.mqttSubscribe.enabled) {
        JsonObject sub = subs.add<JsonObject>();
        String topic = config.subscribeTopic + "/" + device.deviceId + "/" + reg.mqttSubscribe.topicSuffix;
        sub["topic"] = topic;
        sub["qos"] = reg.mqttSubscribe.qos;
        JsonArray regs = sub["registers"].to<JsonArray>();
        regs.add(reg.name);
      }
    }
  }
}

void MqttManager::getPublishTopicsList(JsonArray& topics) const {
  JsonObject topic = topics.add<JsonObject>();

  if (config.customPublishEnabled) {
    topic["topic"] = config.publishTopic;
    topic["interval"] = config.customPublishInterval;
    topic["interval_unit"] = config.customPublishIntervalUnit == 'm' ? "m" : "s";
  } else {
    topic["topic"] = config.defaultTopic;
    topic["interval"] = config.publishInterval;
    topic["interval_unit"] = "s";
  }
}

void MqttManager::getFullStatus(JsonObject& status) {
  // Basic connection info
  status["running"] = running;
  status["mqtt_connected"] = mqttClient.connected();
  status["broker_address"] = config.host;
  status["broker_port"] = config.port;
  status["client_id"] = config.clientId;
  status["queue_size"] = publishQueue.size();
  status["publish_mode"] = config.customPublishEnabled ? "custom" : "default";

  // Statistics object
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

  // Publish topics array
  JsonArray pubTopics = status["publish_topics"].to<JsonArray>();
  getPublishTopicsList(pubTopics);

  // Subscribe control info
  JsonObject subCtrl = status["subscribe_control"].to<JsonObject>();
  subCtrl["custom_subscribe_enabled"] = config.customSubscribeEnabled;
  if (config.customSubscribeEnabled) {
    subCtrl["base_topic"] = config.subscribeTopic;
  }

  // Subscriptions array
  JsonArray subs = status["subscriptions"].to<JsonArray>();
  getSubscriptionsList(subs);
}
```

---

### File 3: CRUDHandler.cpp

**Location:** `Main/CRUDHandler.cpp`

#### Addition: get_mqtt_status Control Handler (line ~1523-1544)

```cpp
// === v1.3.0: MQTT STATUS API (Desktop App MQTT Monitor) ===
controlHandlers["get_mqtt_status"] = [this](BLEManager* manager,
                                           const JsonDocument& command) {
  MqttManager* mqttMgr = MqttManager::getInstance();
  if (!mqttMgr) {
    manager->sendError("MQTT manager not initialized", "control");
    return;
  }

  // Create response document
  auto response = make_psram_unique<JsonDocument>();
  (*response)["status"] = "ok";
  (*response)["command"] = "get_mqtt_status";

  // Get full MQTT status
  JsonObject data = (*response)["data"].to<JsonObject>();
  mqttMgr->getFullStatus(data);

  // Send response via BLE
  manager->sendResponse(*response);
  LOG_CRUD_INFO("[CRUD] MQTT status sent");
};
```

---

## API Summary

### New BLE Command

| Operation | Type | Description |
|-----------|------|-------------|
| `control` | `get_mqtt_status` | Get complete MQTT status and statistics |

### Request

```json
{
  "op": "control",
  "type": "get_mqtt_status"
}
```

### Response

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
    "publish_topics": [...],
    "subscribe_control": {...},
    "subscriptions": [...]
  }
}
```

---

## Memory Considerations

- Statistics struct: ~48 bytes (fixed)
- Response JSON: ~500-2000 bytes depending on topic count
- Uses PSRAM for response document to avoid heap fragmentation

---

## Testing Checklist

- [ ] Verify `get_mqtt_status` returns correct response format
- [ ] Verify `publish_success_count` increments on successful publish
- [ ] Verify `publish_fail_count` increments on failed publish
- [ ] Verify `subscribe_received_count` increments on subscribe message
- [ ] Verify `connection_uptime_ms` calculates correctly
- [ ] Verify `reconnect_count` increments on reconnection
- [ ] Verify `publish_topics` contains correct topic list
- [ ] Verify `subscriptions` contains correct subscription list
- [ ] Test with Desktop App MQTT Monitor page

---

## Backward Compatibility

- **Fully backward compatible** - No existing commands modified
- New command `get_mqtt_status` added alongside existing commands
- Existing apps will continue to work without modification

---

## Bug Fixes (v1.3.0-patch1)

### Fix 1: Timestamp "Last publish/message" menampilkan nilai salah

**Problem:**
Desktop app menghitung `DateTime.now() - millis()` yang menghasilkan nilai huge karena:
- `DateTime.now()` = Unix timestamp (e.g., 1736678400000 ms)
- `millis()` = waktu sejak boot (e.g., 60000 ms)
- Hasil: nilai negatif huge yang salah

**Solution:**
1. Firmware: Tambah field `gateway_uptime_ms = millis()` di response
2. Desktop: Hitung `gateway_uptime_ms - last_publish_timestamp` untuk mendapat "X seconds ago"

**Code Added (MqttManager.cpp line ~2231):**
```cpp
// v1.3.0: Add gateway uptime for accurate "time ago" calculation
statsObj["gateway_uptime_ms"] = millis();
```

### Fix 2: Interval menampilkan "10000s" instead of "10s"

**Problem:**
- Firmware menyimpan interval dalam milliseconds (10000ms untuk 10s)
- Tapi mengembalikan nilai ms dengan unit "s" → tampil "10000s"

**Solution:**
Tambah fungsi `convertFromMilliseconds()` untuk convert balik ke unit asli.

**Code Added (MqttManager.cpp line ~38-54):**
```cpp
// v1.3.0: Helper function: Convert milliseconds back to original unit for display
static uint32_t convertFromMilliseconds(uint32_t intervalMs, const String& unit) {
  String unitLower = unit;
  unitLower.toLowerCase();

  if (unitLower == "ms" || unitLower == "millisecond" ||
      unitLower == "milliseconds" || unitLower.isEmpty()) {
    return intervalMs;
  } else if (unitLower == "s" || unitLower == "sec" || unitLower == "secs" ||
             unitLower == "second" || unitLower == "seconds") {
    return intervalMs / 1000;
  } else if (unitLower == "m" || unitLower == "min" || unitLower == "mins" ||
             unitLower == "minute" || unitLower == "minutes") {
    return intervalMs / 60000;
  }
  return intervalMs;
}
```

**Code Updated (getPublishTopicsList):**
```cpp
// Before: topicObj["interval"] = defaultInterval;  // Returns 10000
// After:
topicObj["interval"] = convertFromMilliseconds(defaultInterval, defaultIntervalUnit);  // Returns 10
```

---

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.3.0 | 2026-01-12 | Initial MQTT Monitor API implementation |
| 1.3.0-patch1 | 2026-01-12 | Fix timestamp calculation, fix interval unit display |

---

```
================================================================================
Copyright (c) 2026 PT Surya Inovasi Prioritas (SURIOTA)
All Rights Reserved
================================================================================
```
